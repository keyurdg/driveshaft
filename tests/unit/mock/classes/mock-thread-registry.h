#ifndef incl_DRIVESHAFT_MOCK_THREAD_REGISTRY_H_
#define incl_DRIVESHAFT_MOCK_THREAD_REGISTRY_H_

#include "thread-registry.h"

namespace mock {
namespace classes {

class MockThreadRegistry : public Driveshaft::ThreadRegistryInterface {
public:
    void registerThread(const std::string& pool, std::thread::id tid) noexcept {}
    void unregisterThread(const std::string& pool, std::thread::id tid) noexcept {}
    uint32_t poolCount(const std::string& pool) noexcept { return 0; }
    bool sendShutdown(const std::string& pool, uint32_t count) noexcept { return true; }
    bool shouldShutdown(std::thread::id tid) noexcept { return false; }
    void setThreadState(std::thread::id tid, const std::string& state) noexcept {}
    Driveshaft::ThreadMap getThreadMap() noexcept { return Driveshaft::ThreadMap(); }
};

} // namespace classes
} // namespace mock

#endif // incl_DRIVESHAFT_MOCK_THREAD_REGISTRY_H_
