#include "thread-loop.h"
#include "gearman-client.h"

namespace Driveshaft {

ThreadLoop::ThreadLoop(ThreadRegistryPtr registry,
                       const std::string& pool,
                       const StringSet& server_list,
                       int64_t timeout,
                       const StringSet& jobs_list,
                       const std::string& http_uri) noexcept
                       : m_registry(registry)
                       , m_pool(pool)
                       , m_server_list(server_list)
                       , m_timeout(timeout)
                       , m_jobs_list(jobs_list)
                       , m_http_uri(http_uri) {
    LOG4CXX_DEBUG(ThreadLogger, "Starting ThreadLoop for " << m_pool);
    m_registry->registerThread(m_pool, std::this_thread::get_id());
}

ThreadLoop::~ThreadLoop() noexcept {
    LOG4CXX_DEBUG(ThreadLogger, "Stopping ThreadLoop for " << m_pool);
    m_registry->unregisterThread(m_pool, std::this_thread::get_id());
}

static const int64_t MAX_THREAD_LOOP_ATTEMPTS = 5;
static const int64_t THREAD_LOOP_RETRY_SLEEP_DURATION = 10; // Seconds

void ThreadLoop::run(uint32_t attempts) noexcept {
    if (attempts > MAX_THREAD_LOOP_ATTEMPTS) {
        LOG4CXX_ERROR(ThreadLogger, "Exceeded max loop attempts");
        return;
    }

    try {
        GearmanClient gearclient(m_server_list, m_timeout, m_jobs_list, m_http_uri);

        while (true) {
            // First check if we should shutdown
            if (g_force_shutdown || m_registry->shouldShutdown(std::this_thread::get_id())) {
                LOG4CXX_INFO(ThreadLogger, "Shutdown requested. Exiting loop");
                break;
            }

            // Do real work
            gearclient.run();
        }
    } catch (GearmanClientException& e) {
        LOG4CXX_ERROR(ThreadLogger, "Caught GearmanClientException: " << e.what() << ". Retriable: " << e.retriable());
        if (e.retriable()) {
            LOG4CXX_INFO(ThreadLogger, "Sleeping for " << THREAD_LOOP_RETRY_SLEEP_DURATION << " seconds before retrying");
            std::this_thread::sleep_for(std::chrono::seconds(THREAD_LOOP_RETRY_SLEEP_DURATION));
            return run(++attempts);
        }
    } catch (std::exception& e) {
        LOG4CXX_ERROR(ThreadLogger, "Caught unhandled exception in ThreadLoop::run. Details: " << e.what());
    }

    return;
}

} // namespace Driveshaft
