#ifndef incl_DRIVESHAFT_MOCK_METRIC_PROXY_H_
#define incl_DRIVESHAFT_MOCK_METRIC_PROXY_H_

#include "metric-proxy.h"

namespace mock {
namespace classes {

class MockMetricProxy : public Driveshaft::MetricProxyInterface {
public:
    MockMetricProxy() noexcept 
        : m_job_successes_count()
        , m_job_successes_delay_sum()
        , m_job_http_error_count()
        , m_job_error_count()
        , m_job_timeout_count() {
    }
    ~MockMetricProxy() noexcept {}

    void reset() {
        m_job_successes_count.clear();
        m_job_successes_delay_sum.clear();
        m_job_http_error_count.clear();
        m_job_timeout_count.clear();
        m_job_error_count.clear();
    }

    /* Implementation of the MetricProxyInterface */
    void reportJobSuccess(const std::string &pool_name, const std::string &function_name, double duration) noexcept override {
        // It looks like addressing a new entry in these maps initializes the entry to zero...
        // I wish I knew this was a feature, not a fluke
        const std::pair<std::string, std::string> key = std::make_pair(pool_name, function_name);
        m_job_successes_count[key] += 1;
        m_job_successes_delay_sum[key] += duration;
    }

    void reportHttpJobError(const std::string &pool_name, const std::string &function_name, uint16_t http_status) noexcept override {
        std::tuple<std::string, std::string, uint16_t> key = std::make_tuple(pool_name, function_name, http_status);
        m_job_http_error_count[key] += 1;
    }

    void reportJobError(const std::string &pool_name, const std::string &function_name) noexcept override {
        const std::pair<std::string, std::string> key = std::make_pair(pool_name, function_name);
        m_job_error_count[key] += 1;
    }

    void reportJobTimeout(const std::string &pool_name, const std::string &function_name) noexcept override {
        const std::pair<std::string, std::string> key = std::make_pair(pool_name, function_name);
        m_job_timeout_count[key] += 1;
    }

    /* Here on below, an interface for validation from test cases */
    uint32_t getJobSuccessesCount(const std::string& pool_name, const std::string& function_name) {
        return m_job_successes_count[std::make_pair(pool_name, function_name)];
    }

    double getJobSuccessesDelaySum(const std::string& pool_name, const std::string& function_name) {
        return m_job_successes_delay_sum[std::make_pair(pool_name, function_name)];
    }

    uint32_t getJobHttpErrorCount(const std::string& pool_name, const std::string& function_name, uint16_t http_status) {
        std::tuple<std::string, std::string, uint16_t> key = std::make_tuple(pool_name, function_name, http_status);
        return m_job_http_error_count[key];
    }

    uint32_t getJobTimeoutCount(const std::string& pool_name, const std::string& function_name) {
        return m_job_timeout_count[std::make_pair(pool_name, function_name)];
    }

    uint32_t getJobErrorCount(const std::string& pool_name, const std::string& function_name) {
        return m_job_error_count[std::make_pair(pool_name, function_name)];
    }

private:
    std::map<std::pair<std::string,std::string>, uint32_t> m_job_successes_count;
    std::map<std::pair<std::string,std::string>, double> m_job_successes_delay_sum;

    std::map<std::tuple<std::string, std::string, uint16_t>, uint32_t> m_job_http_error_count;
    std::map<std::pair<std::string,std::string>, uint32_t> m_job_timeout_count;
    std::map<std::pair<std::string,std::string>, uint32_t> m_job_error_count;
};

} // namespace classes
} // namespace mock

namespace Driveshaft {
// MockMetricProxyPtr is used in the test cases to allow access to the testing interface above
typedef std::shared_ptr<mock::classes::MockMetricProxy> MockMetricProxyPtr;
}

#endif // incl_DRIVESHAFT_MOCK_METRIC_PROXY_H_
