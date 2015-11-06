#include <fstream>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/filesystem.hpp>
#include "driveshaft-config.h"

namespace Driveshaft {

namespace cfgkeys {
static std::string GEARMAN_SERVERS_LIST = "gearman_servers_list";
static std::string POOLS_LIST = "pools_list";
static std::string POOL_WORKER_COUNT = "worker_count";
static std::string POOL_JOB_LIST = "jobs_list";
static std::string POOL_JOB_PROCESSING_URI = "job_processing_uri";
}

DriveshaftConfig::DriveshaftConfig() noexcept :
    m_config_filename(),
    m_server_list(),
    m_pool_map(),
    m_load_time(0) {
}

bool DriveshaftConfig::load(const std::string &config_filename, Json::CharReader::Factory &parser_factory) {
    if (!this->needsConfigUpdate(config_filename)) {
        return false;
    }

    LOG4CXX_INFO(MainLogger, "Reloading config");
    this->m_config_filename = config_filename;

    std::string config_file_contents(this->fetchFileContents(config_filename));
    return this->parseConfig(config_file_contents, parser_factory);
}

bool DriveshaftConfig::parseConfig(const std::string &config_data, Json::CharReader::Factory &parser_factory) {
    Json::Value tree;
    std::string parse_errors;
    auto json_parser = parser_factory.newCharReader();
    if (!json_parser->parse(config_data.data(),
                            config_data.data() + config_data.length(),
                            &tree, &parse_errors)) {
        LOG4CXX_ERROR(MainLogger, "Failed to parse " << this->m_config_filename <<
                                  ". Errors: " << parse_errors);
        throw std::runtime_error("config parse failure");
    }

    if (!this->validateConfigNode(tree)) {
        throw std::runtime_error("config parse failure");
    }

    this->m_load_time = std::time(nullptr);
    this->m_server_list.clear();
    this->m_pool_map.clear();

    this->parseServerList(tree);
    this->parsePoolList(tree);

    return true;
}

void DriveshaftConfig::supersede(DriveshaftConfig &old, PoolWatcher &watcher) const {
    StringSet pools_to_shut;
    std::tie(pools_to_shut, std::ignore) = old.compare(*this);
    for (const auto &pool : pools_to_shut) {
        old.clearWorkerCount(pool, watcher);
    }

    for (auto const &i : this->m_pool_map) {
        const auto &pool_name = i.first;
        const auto &pool_data = i.second;
        watcher.inform(pool_data.worker_count, pool_name, this->m_server_list,
                       pool_data.job_list, pool_data.job_processing_uri);
    }
}

void DriveshaftConfig::clearWorkerCount(const std::string &pool_name, PoolWatcher &watcher) {
    auto pool_iter = this->m_pool_map.find(pool_name);
    if (pool_iter == this->m_pool_map.end()) {
        LOG4CXX_ERROR(MainLogger, "clearWorkerCounts requested, " << pool_name << " does not exist in config");
        throw std::runtime_error("invalid config detected in clearWorkerCounts");
    }

    auto &pool_data = pool_iter->second;
    pool_data.worker_count = 0;
    watcher.inform(0, pool_name, this->m_server_list,
                   pool_data.job_list, pool_data.job_processing_uri);
}

void DriveshaftConfig::clearAllWorkerCounts(PoolWatcher &watcher) {
    for (const auto &i : this->m_pool_map) {
        this->clearWorkerCount(i.first, watcher);
    }
}

std::pair<StringSet, StringSet> DriveshaftConfig::compare(const DriveshaftConfig &that) const noexcept {
    LOG4CXX_DEBUG(MainLogger, "Beginning config compare");

    StringSet current_pool_names, latest_pool_names;
    boost::copy(m_pool_map | boost::adaptors::map_keys,
                std::inserter(current_pool_names, current_pool_names.begin()));
    boost::copy(that.m_pool_map | boost::adaptors::map_keys,
                std::inserter(latest_pool_names, latest_pool_names.begin()));

    if (this->m_server_list != that.m_server_list) {
        // Everything needs restarting
        return std::pair<StringSet, StringSet>(std::move(current_pool_names),
                                               std::move(latest_pool_names));
    } else {
        StringSet pools_turn_off, pools_turn_on;

        // See what's present in current and missing in new. This is to be turned off.
        std::set_difference(current_pool_names.begin(), current_pool_names.end(),
                            latest_pool_names.begin(), latest_pool_names.end(),
                            std::inserter(pools_turn_off, pools_turn_off.begin()));

        // Reverse of above. See what's to be turned on.
        std::set_difference(latest_pool_names.begin(), latest_pool_names.end(),
                            current_pool_names.begin(), current_pool_names.end(),
                            std::inserter(pools_turn_on, pools_turn_on.begin()));

        // Check for changed jobs or processing URI
        for (const auto& i : m_pool_map) {
            auto found = that.m_pool_map.find(i.first);
            if (found != that.m_pool_map.end()) {
                bool should_restart = false;
                if (found->second.job_processing_uri != i.second.job_processing_uri) {
                    should_restart = true;
                } else {
                    const auto &lhs_jobs_set = i.second.job_list;
                    const auto &rhs_jobs_set = found->second.job_list;
                    StringSet diff;
                    std::set_symmetric_difference(lhs_jobs_set.begin(), lhs_jobs_set.end(),
                                                  rhs_jobs_set.begin(), rhs_jobs_set.end(),
                                                  std::inserter(diff, diff.begin()));
                    if (diff.size()) {
                        should_restart = true;
                    }
                }

                if (should_restart) {
                    pools_turn_off.insert(i.first);
                    pools_turn_on.insert(i.first);
                }
            }
        }

        return std::pair<StringSet, StringSet>(std::move(pools_turn_off), std::move(pools_turn_on));
    }
}

void DriveshaftConfig::parseServerList(const Json::Value &node) {
    const auto &servers_list = node[cfgkeys::GEARMAN_SERVERS_LIST];
    for (auto i = servers_list.begin(); i != servers_list.end(); ++i) {
        if (!i->isString()) {
            LOG4CXX_ERROR(MainLogger, cfgkeys::GEARMAN_SERVERS_LIST << " does not contain strings");
            throw std::runtime_error("config server list type failure");
        }

        const auto &name = i->asString();
        this->m_server_list.insert(name);
        LOG4CXX_DEBUG(MainLogger, "Read server: " << name);
    }
}

void DriveshaftConfig::parsePoolList(const Json::Value &node) {
    const auto &pools_list = node[cfgkeys::POOLS_LIST];
    for (auto i = pools_list.begin(); i != pools_list.end(); ++i) {
        const auto &pool_name = i.name();
        const auto &pool_node = *i;

        if (!pool_node.isMember(cfgkeys::POOL_WORKER_COUNT) ||
            !pool_node.isMember(cfgkeys::POOL_JOB_LIST) ||
            !pool_node.isMember(cfgkeys::POOL_JOB_PROCESSING_URI) ||
            !pool_node[cfgkeys::POOL_WORKER_COUNT].isUInt() ||
            !pool_node[cfgkeys::POOL_JOB_LIST].isArray() ||
            !pool_node[cfgkeys::POOL_JOB_PROCESSING_URI].isString()) {
            LOG4CXX_ERROR(
                MainLogger,
                "Config (" << m_config_filename << ") has invalid " <<
                cfgkeys::POOLS_LIST << " elements: (" <<
                cfgkeys::POOL_WORKER_COUNT << ", " <<
                cfgkeys::POOL_JOB_PROCESSING_URI << ", " <<
                cfgkeys::POOL_JOB_LIST << ")"
            );

            throw std::runtime_error("config jobs list parse failure");
        }

        auto &pool_data = this->m_pool_map[pool_name];
        pool_data.worker_count = pool_node[cfgkeys::POOL_WORKER_COUNT].asUInt();
        pool_data.job_processing_uri = pool_node[cfgkeys::POOL_JOB_PROCESSING_URI].asString();
        LOG4CXX_DEBUG(
            MainLogger,
            "Read pool: " << pool_name << " with count " <<
            pool_data.worker_count << " and URI " << pool_data.job_processing_uri
        );

        const auto &job_list = pool_node[cfgkeys::POOL_JOB_LIST];
        for (auto j = job_list.begin(); j != job_list.end(); ++j) {
            if (!j->isString()) {
                LOG4CXX_ERROR(MainLogger, cfgkeys::POOL_JOB_LIST << " does not contain strings");
            }

            const auto &name = j->asString();
            pool_data.job_list.insert(name);
            LOG4CXX_DEBUG(MainLogger, "Pool " << pool_name << " adding job " << name);
        }
    }
}

bool DriveshaftConfig::needsConfigUpdate(const std::string &new_config_filename) const {
    boost::filesystem::path config_path(new_config_filename);
    std::time_t modified_time = boost::filesystem::last_write_time(config_path);
    if (modified_time == (std::time_t)(-1)) {
        LOG4CXX_ERROR(
            MainLogger,
            "Unable to ascertain config mtime " << new_config_filename <<
            ". Error: " << std::to_string(errno)
        );
        throw std::runtime_error("config stat failure");
    }

    return modified_time > this->m_load_time;
}

std::string DriveshaftConfig::fetchFileContents(const std::string &filename) const {
    std::ifstream config_filestream(filename, std::ios::in | std::ios::binary);
    if (config_filestream.fail()) {
        LOG4CXX_ERROR(MainLogger, "Unable to load file " << filename);
        throw std::runtime_error("config read failure");
    }

    std::ostringstream contents;
    contents << config_filestream.rdbuf();
    config_filestream.close();
    return contents.str();
}

bool DriveshaftConfig::validateConfigNode(const Json::Value &node) const {
    using namespace cfgkeys;
    if (!node.isMember(GEARMAN_SERVERS_LIST) ||
        !node.isMember(POOLS_LIST)) {
        LOG4CXX_ERROR(
            MainLogger,
            "Config is missing one or more elements (" <<
              GEARMAN_SERVERS_LIST << ", " <<
              POOLS_LIST << ")"
        );

        return false;
    }

    if (!node[GEARMAN_SERVERS_LIST].isArray() ||
        !node[POOLS_LIST].isObject()) {
        LOG4CXX_ERROR(
            MainLogger,
            "Config has one or more malformed elements (" <<
              GEARMAN_SERVERS_LIST << ", " <<
              POOLS_LIST << ")"
        );

        return false;
    }

    return true;
}

} // namespace Driveshaft
