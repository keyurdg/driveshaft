#ifndef incl_DRIVESHAFT_MOCK_METRIC_PROXY_H_
#define incl_DRIVESHAFT_MOCK_METRIC_PROXY_H_

#include "metric-proxy.h"

namespace mock {
namespace classes {
    
using std::chrono::high_resolution_clock;
using std::chrono::duration;
using std::chrono::duration_cast;
typedef high_resolution_clock::time_point time_point;

typedef std::pair<std::string, std::string> pool_and_function;
typedef std::tuple<std::string, std::string, uint16_t> pool_function_and_status;
typedef std::stack<time_point> time_points;

class MockMetricProxy : public Driveshaft::MetricProxyInterface {
public:
    MockMetricProxy() noexcept 
        : m_job_successes_count()
        , m_job_successes_delay_sum()
        , m_job_http_error_count()
        , m_job_timeout_count()
        , m_job_error_count() {
    }
    ~MockMetricProxy() noexcept override = default;

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
        const pool_and_function key = make_pf(pool_name, function_name);
        m_job_successes_count[key] += 1;
        m_job_successes_delay_sum[key] += duration;
    }

    void reportHttpJobError(const std::string &pool_name, const std::string &function_name, uint16_t http_status) noexcept override {
        m_job_http_error_count[std::make_tuple(pool_name, function_name, http_status)] += 1;
    }

    void reportJobError(const std::string &pool_name, const std::string &function_name) noexcept override {
        m_job_error_count[make_pf(pool_name, function_name)] += 1;
    }
    
    void reportJobTimeout(const std::string &pool_name, const std::string &function_name) noexcept override {
        m_job_timeout_count[make_pf(pool_name, function_name)] += 1;
    }
    
    void reportThreadStarted(const std::string &pool_name) noexcept override{
        m_thread_starts[pool_name].push(high_resolution_clock::now());
    }
    void reportThreadEnded(const std::string &pool_name) noexcept override {
        m_thread_ends[pool_name].push(high_resolution_clock::now());
    }
    void reportThreadStartingWork(const std::string &pool_name, const std::string &function_name) noexcept override {
        m_work_starts[make_pf(pool_name, function_name)].push(high_resolution_clock::now());
    }
    void reportThreadWorkComplete(const std::string &pool_name, const std::string &function_name) noexcept override {
        m_work_ends[make_pf(pool_name, function_name)].push(high_resolution_clock::now());
    }
    
    /* Here on below, an interface for validation from test cases */
    uint32_t getJobSuccessesCount(const std::string& pool_name, const std::string& function_name) {
        return m_job_successes_count[make_pf(pool_name, function_name)];
    }

    double getJobSuccessesDelaySum(const std::string& pool_name, const std::string& function_name) {
        return m_job_successes_delay_sum[make_pf(pool_name, function_name)];
    }

    uint32_t getJobHttpErrorCount(const std::string& pool_name, const std::string& function_name, uint16_t http_status) {
        pool_function_and_status key = std::make_tuple(pool_name, function_name, http_status);
        return m_job_http_error_count[key];
    }

    uint32_t getJobTimeoutCount(const std::string& pool_name, const std::string& function_name) {
        return m_job_timeout_count[make_pf(pool_name, function_name)];
    }

    uint32_t getJobErrorCount(const std::string& pool_name, const std::string& function_name) {
        return m_job_error_count[make_pf(pool_name, function_name)];
    }

    time_point popThreadStart(const std::string& pool_name) {
        if (m_thread_starts[pool_name].empty()) throw std::runtime_error("no thread starts recorded");
        auto retval = m_thread_starts[pool_name].top();
        m_thread_starts[pool_name].pop();
        return retval;
    }
    time_point popThreadEnd(const std::string& pool_name) {
        if (m_thread_ends[pool_name].empty()) throw std::runtime_error("no thread ends recorded");
        auto retval = m_thread_ends[pool_name].top();
        m_thread_ends[pool_name].pop();
        return retval;
    }

    time_point popWorkStart(const std::string& pool_name, const std::string& function_name) {
        auto pf = make_pf(pool_name, function_name);
        if (m_work_starts[pf].empty()) throw std::runtime_error("no thread starts recorded");
        auto retval = m_work_starts[pf].top();
        m_work_starts[pf].pop();
        return retval;
    }

    time_point popWorkEnd(const std::string& pool_name, const std::string& function_name) {
        auto pf = make_pf(pool_name, function_name);
        if (m_work_ends[pf].empty()) throw std::runtime_error("no thread starts recorded");
        auto retval = m_work_ends[pf].top();
        m_work_ends[pf].pop();
        return retval;
    }

private:
    std::map<pool_and_function, uint32_t> m_job_successes_count;
    std::map<pool_and_function, double> m_job_successes_delay_sum;

    std::map<pool_function_and_status, uint32_t> m_job_http_error_count;
    std::map<pool_and_function, uint32_t> m_job_timeout_count;
    std::map<pool_and_function, uint32_t> m_job_error_count;

    std::map<std::string, time_points> m_thread_starts;
    std::map<std::string, time_points> m_thread_ends;
    std::map<pool_and_function, time_points> m_work_starts;
    std::map<pool_and_function, time_points> m_work_ends;

    static pool_and_function make_pf(const std::string &pool_name, const std::string &function_name) { return std::make_pair(pool_name, function_name); }
};

} // namespace classes
} // namespace mock

namespace Driveshaft {
// MockMetricProxyPtr is used in the test cases to allow access to the testing interface above
typedef std::shared_ptr<mock::classes::MockMetricProxy> MockMetricProxyPtr;
}

#endif // incl_DRIVESHAFT_MOCK_METRIC_PROXY_H_
