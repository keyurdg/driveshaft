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

// #include <string>
// #include <map>
// #include <set>
// #include <thread>
// #include <memory>
// #include <mutex>
#include "common-defs.h"

namespace Driveshaft {

class MetricProxyInterface {
public:
    virtual ~MetricProxyInterface() noexcept {}

    virtual void
    reportJobSuccess(const std::string &pool_name, const std::string &function_name, double duration) noexcept = 0;
};

class MetricProxy : public MetricProxyInterface {
public:
    MetricProxy() noexcept;
    ~MetricProxy() noexcept;

    void reportJobSuccess(const std::string &pool_name, const std::string &function_name, double duration) noexcept;

private:
    MetricProxy(const MetricProxy&) = delete;
    MetricProxy(MetricProxy&&) = delete;
    MetricProxy& operator=(const MetricProxy&) = delete;
    MetricProxy& operator=(const MetricProxy&&) = delete;
};

typedef std::shared_ptr<MetricProxyInterface> MetricProxyPtr;
}

#endif // incl_DRIVESHAFT_METRIC_PROXY_H_
