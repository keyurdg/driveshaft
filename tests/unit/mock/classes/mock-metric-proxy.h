#ifndef incl_DRIVESHAFT_MOCK_METRIC_PROXY_H_
#define incl_DRIVESHAFT_MOCK_METRIC_PROXY_H_

#include "metric-proxy.h"

namespace mock {
namespace classes {

class MockMetricProxy : public Driveshaft::MetricProxyInterface {
public:
    MockMetricProxy() noexcept : m_job_successes_reported()  {
    }
    ~MockMetricProxy() noexcept {}

    /* Implementation of the MetricProxyInterface */
    void reportJobSuccess(const std::string &pool_name, const std::string &function_name, double duration) noexcept {
        // It looks like addressing a new entry initializes it to zero... I hope that
        // isn't only true in debug builds.  Oh well, it's test code and works for now
        m_job_successes_reported[std::make_pair(pool_name, function_name)] += 1;
    }

    /* Here on below, an interface for validation from test cases */
    uint32_t getCountJobSuccessesReported(const std::string& pool_name, const std::string& function_name) {
        return m_job_successes_reported[std::make_pair(pool_name, function_name)];
    }

private:
    std::map<std::pair<std::string,std::string>, uint32_t> m_job_successes_reported;
};

} // namespace classes
} // namespace mock

namespace Driveshaft {
// MockMetricProxyPtr is used in the test cases to allow access to the testing interface above
typedef std::shared_ptr<mock::classes::MockMetricProxy> MockMetricProxyPtr;
}

#endif // incl_DRIVESHAFT_MOCK_METRIC_PROXY_H_
