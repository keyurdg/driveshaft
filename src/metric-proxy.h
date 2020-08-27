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

#ifndef incl_DRIVESHAFT_METRIC_PROXY_H_
#define incl_DRIVESHAFT_METRIC_PROXY_H_

#include <string>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/histogram.h>
#include <prometheus/gauge.h>
#include <stack>

#include "common-defs.h"

namespace Driveshaft {

class MetricProxyInterface {
public:
    virtual ~MetricProxyInterface() noexcept = default;

    virtual void reportJobSuccess(const std::string &pool_name, const std::string &function_name, double duration) noexcept = 0;
    virtual void reportHttpJobError(const std::string &pool_name, const std::string &function_name, uint16_t http_status) noexcept = 0;
    virtual void reportJobTimeout(const std::string &pool_name, const std::string &function_name) noexcept = 0;
    virtual void reportJobError(const std::string &pool_name, const std::string &function_name) noexcept = 0;

    virtual void reportThreadStarted(const std::string &pool_name) noexcept = 0;
    virtual void reportThreadEnded(const std::string &pool_name) noexcept = 0;
    virtual void reportThreadStartingWork(const std::string &pool_name, const std::string &function_name) noexcept = 0;
    virtual void reportThreadWorkComplete(const std::string &pool_name, const std::string &function_name) noexcept = 0;
};

class MetricProxy : public MetricProxyInterface {
public:
    explicit MetricProxy(const std::string &metricAddress) noexcept;
    ~MetricProxy() noexcept override;

    void reportJobSuccess(const std::string &pool_name, const std::string &function_name, double duration) noexcept override;
    void reportHttpJobError(const std::string &pool_name, const std::string &function_name, uint16_t http_status) noexcept override;
    void reportJobTimeout(const std::string &pool_name, const std::string &function_name) noexcept override;
    void reportJobError(const std::string &pool_name, const std::string &function_name) noexcept override;

    void reportThreadStarted(const std::string &pool_name) noexcept override;
    void reportThreadEnded(const std::string &pool_name) noexcept override;
    void reportThreadStartingWork(const std::string &pool_name, const std::string &function_name) noexcept override;
    void reportThreadWorkComplete(const std::string &pool_name, const std::string &function_name) noexcept override;


    // Disable copying
    MetricProxy(const MetricProxy&) = delete;
    MetricProxy(MetricProxy&&) = delete;
    MetricProxy& operator=(const MetricProxy&) = delete;
    MetricProxy& operator=(const MetricProxy&&) = delete;

private:
    prometheus::Exposer m_exporter;
    std::shared_ptr<prometheus::Registry> m_registry;

    const prometheus::Histogram::BucketBoundaries m_job_duration_bucket_boundaries =
            prometheus::Histogram::BucketBoundaries{0.01, 0.1, 1, 10, 100, 1000};

    prometheus::Family<prometheus::Histogram> &m_job_duration_family = prometheus::BuildHistogram()
            .Name("driveshaft_job_duration")
            .Help("histogram of the execution time of successful jobs run by driveshaft")
            .Labels({})
            .Register(*m_registry);

    prometheus::Family<prometheus::Counter> &m_http_errors_family = prometheus::BuildCounter()
            .Name("driveshaft_http_errors")
            .Help("http errors returned when calling a job")
            .Labels({})
            .Register(*m_registry);

    prometheus::Family<prometheus::Counter> &m_timeouts_family = prometheus::BuildCounter()
            .Name("driveshaft_timeouts")
            .Help("http errors returned when calling a job")
            .Labels({})
            .Register(*m_registry);

    prometheus::Family<prometheus::Counter> &m_errors_family = prometheus::BuildCounter()
            .Name("driveshaft_errors")
            .Help("error when processing a job, includes driveshaft_http_errors and driveshaft_timeouts")
            .Labels({})
            .Register(*m_registry);

    prometheus::Family<prometheus::Gauge> &m_threads_family = prometheus::BuildGauge()
            .Name("driveshaft_threads")
            .Help("tracks threads by pool and function including their working/idle status")
            .Labels({})
            .Register(*m_registry);
};

typedef std::shared_ptr<MetricProxyInterface> MetricProxyPtr;

class MetricProxyPoolWrapper;
typedef std::shared_ptr<MetricProxyPoolWrapper> MetricProxyPoolWrapperPtr;

class MetricProxyPoolWrapper {
public:
    static MetricProxyPoolWrapperPtr wrap(const std::string pool_name, MetricProxyPtr metrics) {
        return MetricProxyPoolWrapperPtr(new MetricProxyPoolWrapper(pool_name, metrics));
    }

    void reportJobSuccess(const std::string &function_name, double duration) noexcept {
        m_metric_proxy->reportJobSuccess(m_pool_name, function_name, duration);
    }

    void reportJobHttpError(const std::string &function_name, uint16_t http_status) noexcept {
        m_metric_proxy->reportHttpJobError(m_pool_name, function_name, http_status);
    }

    virtual void reportJobTimeout(const std::string &function_name) noexcept {
        m_metric_proxy->reportJobTimeout(m_pool_name, function_name);
    }

    void reportJobError(const std::string &function_name) noexcept {
        m_metric_proxy->reportJobError(m_pool_name, function_name);
    }

    void reportThreadStarted() noexcept {
        m_metric_proxy->reportThreadStarted(m_pool_name);
    }

    void reportThreadEnded() noexcept {
        if (m_active_function.empty()) {
            m_metric_proxy->reportThreadEnded(m_pool_name);
        } else {
            std::string &function_name = m_active_function.top();
            m_metric_proxy->reportThreadWorkComplete(m_pool_name, function_name);
            m_active_function.pop();
        }
    }

    void reportThreadStartingWork(const std::string &function_name) noexcept {
        m_metric_proxy->reportThreadStartingWork(m_pool_name, function_name);
        m_active_function.push(function_name);
    }

    void reportThreadWorkComplete() noexcept {
        if (m_active_function.empty())
            return;
        std::string &function_name = m_active_function.top();
        m_metric_proxy->reportThreadWorkComplete(m_pool_name, function_name);
        m_active_function.pop();
    }

private:
    const std::string m_pool_name;
    MetricProxyPtr m_metric_proxy;

    std::stack<std::string> m_active_function;

    MetricProxyPoolWrapper(const std::string pool_name, MetricProxyPtr metrics) noexcept
        : m_pool_name(pool_name)
        , m_metric_proxy(metrics)
        , m_active_function() {
    }
};

}

#endif // incl_DRIVESHAFT_METRIC_PROXY_H_
