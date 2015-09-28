#ifndef incl_DRIVESHAFT_THREAD_LOOP_H_
#define incl_DRIVESHAFT_THREAD_LOOP_H_

#include <string>
#include <map>
#include <set>
#include <thread>
#include <memory>
#include <mutex>
#include "common-defs.h"
#include "thread-registry.h"

namespace Driveshaft {

class ThreadLoop {

public:
    ThreadLoop(ThreadRegistryPtr registry,
               const std::string& pool,
               const StringSet& server_list,
               const StringSet& jobs_list,
               const std::string& http_uri) noexcept;
    ~ThreadLoop() noexcept;
    void run(uint32_t attempts) noexcept;

private:
    ThreadLoop() = delete;
    ThreadLoop(const ThreadLoop&) = delete;
    ThreadLoop(ThreadLoop&&) = delete;
    ThreadLoop& operator=(const ThreadLoop&) = delete;
    ThreadLoop& operator=(const ThreadLoop&&) = delete;

    ThreadRegistryPtr m_registry;
    const std::string& m_pool;
    const StringSet& m_server_list;
    const StringSet& m_jobs_list;
    const std::string& m_http_uri;
};

} // namespace Driveshaft

#endif // incl_DRIVESHAFT_THREAD_REGISTRY_H_
