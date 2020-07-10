/*
 * Driveshaft: Gearman worker manager
 *
 * Copyright (c) [2015] [Keyur Govande]
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "metric-proxy.h"

namespace Driveshaft {

MetricProxy::MetricProxy(const char *metricAddress) noexcept
    : m_exporter{metricAddress} // ←-- starts the exporter
    , m_registry(new prometheus::Registry()) {
    LOG4CXX_DEBUG(MainLogger, "Started metric exporter on " << metricAddress);
    m_exporter.RegisterCollectable(m_registry);

//    prometheus::Family<prometheus::Counter> &m_errors_family = prometheus::BuildCounter()
//            .Name("driveshaft_errors")
//            .Help("error when processing a job, includes driveshaft_http_errors and driveshaft_timeouts")
//            .Labels({})
//            .Register(*m_registry);



}

MetricProxy::~MetricProxy() noexcept {
}

void
MetricProxy::reportJobSuccess(const std::string &pool_name, const std::string &function_name, double duration) noexcept {
    auto& metric = m_job_duration_family.Add({{"pool", pool_name},
                                              {"function", function_name}},
                                              m_job_duration_bucket_boundaries);
    metric.Observe(duration);
}

void
MetricProxy::reportHttpJobError(const std::string &pool_name, const std::string &function_name, uint16_t http_status) noexcept {
    const std::string &http_status_str = std::to_string(http_status);
    auto& counter = m_http_errors_family.Add({{"pool", pool_name},
                                              {"function", function_name},
                                              {"http_status", http_status_str}});
    counter.Increment();
}

void MetricProxy::reportJobTimeout(const std::string &pool_name, const std::string &function_name) noexcept {
    auto& counter = m_timeouts_family.Add({{"pool", pool_name},
                                           {"function", function_name}});
    counter.Increment();
}

void MetricProxy::reportJobError(const std::string &pool_name, const std::string &function_name) noexcept {
    auto& counter = m_errors_family.Add({{"pool", pool_name},
                                         {"function", function_name}});
    counter.Increment();
}

void MetricProxy::reportThreadStartingWork(const std::string &pool_name, const std::string &function_name) noexcept {
    auto& active = m_threads_family.Add({{"pool", pool_name},
                                         {"function", function_name},
                                         {"status", "working"}});

    auto& idle = m_threads_family.Add({{"pool", pool_name},
                                       {"status", "idle"}});

    active.Increment();
    idle.Decrement();
}

void MetricProxy::reportThreadWorkComplete(const std::string &pool_name, const std::string &function_name) noexcept {
    auto& active = m_threads_family.Add({{"pool", pool_name},
                                         {"function", function_name},
                                         {"status", "working"}});

    auto& idle = m_threads_family.Add({{"pool", pool_name},
                                       {"status", "idle"}});

    active.Decrement();
    idle.Increment();
}

void MetricProxy::reportThreadStarted(const std::string &pool_name) noexcept {
    auto& idle = m_threads_family.Add({{"pool", pool_name},
                                       {"status", "idle"}});
    idle.Increment();
}

void MetricProxy::reportThreadEnded(const std::string &pool_name) noexcept {
    auto& idle = m_threads_family.Add({{"pool", pool_name},
                                       {"status", "idle"}});
    idle.Decrement();
}

}
