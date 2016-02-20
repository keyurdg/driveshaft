#ifndef incl_DRIVESHAFT_CONFIG_H
#define incl_DRIVESHAFT_CONFIG_H

#include <map>
#include <string>
#include <ctime>
#include "common-defs.h"
#include "dist/json/json.h"

namespace Driveshaft {

class PoolWatcher {
public:
    virtual void inform(uint32_t config_worker_count, const std::string &pool_name,
                        const StringSet &server_list, const StringSet &jobs_list,
                        const std::string &procesing_uri) = 0;
};

class DriveshaftConfig {
public:
    DriveshaftConfig() noexcept;

    bool load(const std::string &config_filename, Json::CharReader::Factory &parser_factory);
    bool parseConfig(const std::string &config_data, Json::CharReader::Factory &parser_factory);

    void supersede(DriveshaftConfig &old, PoolWatcher &watcher) const;

    void clearWorkerCount(const std::string &pool_name, PoolWatcher &watcher);
    void clearAllWorkerCounts(PoolWatcher &watcher);

    std::pair<StringSet, StringSet> compare(const DriveshaftConfig &that) const noexcept;

private:
    void parseServerList(const Json::Value &node);
    void parsePoolList(const Json::Value &node);

    bool needsConfigUpdate(const std::string &new_config_filename) const;
    std::string fetchFileContents(const std::string &filename) const;
    bool validateConfigNode(const Json::Value &node) const;

    typedef struct {
        uint32_t worker_count;
        std::string job_processing_uri;
        StringSet job_list;
    } PoolData;
    typedef std::map<std::string, PoolData> PoolMap;

    std::string m_config_filename;
    StringSet m_server_list;
    PoolMap m_pool_map;
    std::time_t m_load_time;
};

} // namespace Driveshaft
#endif // incl_DRIVESHAFT_CONFIG_H
