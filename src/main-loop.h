#ifndef incl_DRIVESHAFT_MAIN_LOOP_H_
#define incl_DRIVESHAFT_MAIN_LOOP_H_

#include <string>
#include <map>
#include <utility>
#include <time.h>
#include "dist/json/json.h"
#include "common-defs.h"
#include "thread-registry.h"

/* forward declaration so we can friend it to MainLoopImpl below.
 * The friend'ing ensures only main() can create objects of type
 * MainLoopImpl. MainLoopImpl itself can only access the parent
 * constructor and run method and none of the private methods that
 * actually do the work.
 *
 * This whole charade so no users of this header can incorrectly
 * create a stray MainLoop object and call run on it. MainLoop
 * needs to be a singleton in this program.
 */
int main(int argc, char **argv);

namespace Driveshaft {

/* count-of-workers -> [job names] */
typedef struct {
    uint32_t worker_count;
    std::string job_processing_uri;
    StringSet jobs_list;
} PoolData;

typedef std::map<std::string, PoolData> PoolMap;

struct DriveshaftConfig {
    StringSet m_servers_list;
    PoolMap m_pool_map;
    time_t m_load_time;

    std::pair<StringSet, StringSet> compare(const DriveshaftConfig& against) const noexcept;
};

class MainLoop {

protected:
    MainLoop(const std::string& config_file) noexcept;
    void run();
    ~MainLoop() = default;

private:
    bool setupSignals() const noexcept;
    void doShutdown(uint32_t wait) noexcept;
    void modifyPool(const std::string& name) noexcept;
    bool loadConfig(DriveshaftConfig& new_config);
    void checkRunningConfigAndRegistry() noexcept;

    MainLoop() = delete;
    MainLoop(const MainLoop&) = delete;
    MainLoop(MainLoop&&) = delete;
    MainLoop& operator=(const MainLoop&) = delete;
    MainLoop& operator=(const MainLoop&&) = delete;

    std::string m_config_filename;
    DriveshaftConfig m_config;
    ThreadRegistryPtr m_thread_registry;
    std::unique_ptr<Json::CharReader> m_json_parser;
};

class MainLoopImpl : private MainLoop {
public:
    friend int ::main(int argc, char **argv);

private:
    using MainLoop::MainLoop;
};

} // namespace Driveshaft

#endif // incl_DRIVESHAFT_MAIN_REGISTRY_H_
