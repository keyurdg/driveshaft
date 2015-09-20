#ifndef incl_DRIVESHAFT_THREAD_REGISTRY_H_
#define incl_DRIVESHAFT_THREAD_REGISTRY_H_

#include <string>
#include <map>
#include <set>
#include <thread>
#include <memory>
#include <mutex>
#include "common-defs.h"

namespace Driveshaft {

typedef std::map<std::thread::id, bool> ThreadMap;
typedef std::map<std::string, std::set<std::thread::id> > ThreadRegistryStore;

class ThreadRegistry {

public:
    ThreadRegistry() noexcept;
    ~ThreadRegistry() noexcept;

    void registerThread(const std::string& pool, std::thread::id tid) noexcept;
    void unregisterThread(const std::string& pool, std::thread::id tid) noexcept;
    uint32_t poolCount(const std::string& pool) noexcept;
    bool sendShutdown(const std::string& pool, uint32_t count) noexcept;
    bool shouldShutdown(std::thread::id tid) noexcept;

private:
    ThreadRegistry(const ThreadRegistry&) = delete;
    ThreadRegistry(ThreadRegistry&&) = delete;
    ThreadRegistry& operator=(const ThreadRegistry&) = delete;
    ThreadRegistry& operator=(const ThreadRegistry&&) = delete;

    ThreadMap m_thread_map;
    ThreadRegistryStore m_registry_store;
    std::mutex m_mutex;
};

typedef std::shared_ptr<ThreadRegistry> ThreadRegistryPtr;

}

#endif // incl_DRIVESHAFT_THREAD_REGISTRY_H_
