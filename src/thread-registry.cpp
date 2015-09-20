#include "thread-registry.h"
#include <cassert>

namespace Driveshaft {

ThreadRegistry::ThreadRegistry() noexcept : m_thread_map(), m_registry_store(), m_mutex() {
    LOG4CXX_DEBUG(MainLogger, "Starting thread registry");
}

ThreadRegistry::~ThreadRegistry() noexcept {
    assert(m_thread_map.size() == 0);
}

void ThreadRegistry::registerThread(const std::string& pool, std::thread::id tid) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG4CXX_DEBUG(ThreadLogger, "Registering thread for pool " << pool << " with ID " << tid);

    auto& pool_threads = m_registry_store[pool];
    assert(pool_threads.count(tid) == 0);
    pool_threads.insert(tid);

    assert(m_thread_map.count(tid) == 0);
    m_thread_map.insert(std::pair<std::thread::id, bool>(tid, false));
}

void ThreadRegistry::unregisterThread(const std::string& pool, std::thread::id tid) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG4CXX_DEBUG(ThreadLogger, "Unregistering thread for pool " << pool << " with ID " << tid);

    auto& pool_threads = m_registry_store[pool];
    assert(pool_threads.count(tid) == 1);
    pool_threads.erase(tid);

    assert(m_thread_map.count(tid) == 1);
    m_thread_map.erase(tid);
}

uint32_t ThreadRegistry::poolCount(const std::string& pool) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG4CXX_DEBUG(MainLogger, "Getting poolCount for pool " << pool);

    return m_registry_store[pool].size();
}

bool ThreadRegistry::sendShutdown(const std::string& pool, uint32_t count) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG4CXX_INFO(MainLogger, "Sending shutdown to pool " << pool << ". Count " << count);

    const auto& pool_threads = m_registry_store[pool];
    uint32_t msg_sent = 0;

    for (auto tid : pool_threads) {
        assert(m_thread_map.count(tid) == 1);
        if (m_thread_map[tid] == false) {
            m_thread_map[tid] = true;
            ++msg_sent;
            if (msg_sent == count) {
                break;
            }
        }
    }

    LOG4CXX_INFO(MainLogger, "Shutdown sent to " << msg_sent << " members of pool " << pool);

    return msg_sent == count;
}

bool ThreadRegistry::shouldShutdown(std::thread::id tid) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG4CXX_DEBUG(ThreadLogger, "Checking shouldShutdown for ID " << tid);

    assert(m_thread_map.count(tid) == 1);
    return m_thread_map[tid];
}

}
