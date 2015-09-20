#include <iostream>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/adaptors.hpp>
#include <exception>
#include <utility>
#include <mutex>
#include <condition_variable>
#include "common-defs.h"
#include "thread-registry.h"
#include "main-loop.h"
#include "thread-loop.h"

namespace Driveshaft {

MainLoop& MainLoop::getInstance(const std::string& config_file) noexcept {
    static MainLoop singleton(config_file);
    return singleton;
}

MainLoop::MainLoop(const std::string& config_file) noexcept : m_config_filename(config_file), m_config(), m_thread_registry(new ThreadRegistry()) {
    if (setupSignals()) {
        throw std::runtime_error("Unable to setup signals");
    }
}

enum class ShutdownType {
    NO,
    GRACEFUL,
    HARD
};

extern "C" {

static ShutdownType shutdown_type = ShutdownType::NO;

static void shutdown_handler(int signal_arg) {
    if (signal_arg == SIGUSR1) {
        shutdown_type = ShutdownType::GRACEFUL;
    } else {
        shutdown_type = ShutdownType::HARD;
    }
}
}

static const uint32_t LOOP_SLEEP_DURATION = 2; // Seconds
static const uint32_t HARD_SHUTDOWN_WAIT = 10; //seconds
static const uint32_t GRACEFUL_SHUTDOWN_WAIT = 30; //seconds

void MainLoop::doShutdown(uint32_t wait) noexcept {
    for (auto& i : m_config.m_pool_map) {
        i.second.first = 0;
        modifyPool(i.first);
    }

    std::this_thread::sleep_for(std::chrono::seconds(wait));
}

void MainLoop::run() noexcept {
    auto operate = [this](const StringSet &list) {
        for (auto const& i : list) {
            modifyPool(i);
        }
    };

    while(true) {
        switch (shutdown_type) {
        case ShutdownType::GRACEFUL:
            return doShutdown(GRACEFUL_SHUTDOWN_WAIT);

        case ShutdownType::HARD:
            return doShutdown(HARD_SHUTDOWN_WAIT);

        case ShutdownType::NO:
            break;
        }

        DriveshaftConfig new_config;
        if (loadConfig(new_config)) {
            StringSet pools_to_shut, pools_to_start;
            std::tie(pools_to_shut, pools_to_start) = m_config.compare(new_config);

            // pools to shut should operate on current config
            for (auto const& i : pools_to_shut) {
                auto pooldata = m_config.m_pool_map.find(i);
                if (pooldata != m_config.m_pool_map.end()) {
                    pooldata->second.first = 0;
                    modifyPool(i);
                }
            }

            // pools to start will operate on the new config
            m_config = new_config;
        } else {
            LOG4CXX_DEBUG(MainLogger, "Config has not changed");
        }

        // Check the number of workers actually running matches the config
        StringSet current_pool_names;
        boost::copy(m_config.m_pool_map | boost::adaptors::map_keys,
                    std::inserter(current_pool_names, current_pool_names.begin()));
        operate(current_pool_names);

        std::this_thread::sleep_for(std::chrono::seconds(LOOP_SLEEP_DURATION));
    }
}

bool MainLoop::setupSignals() const noexcept {
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));

  sa.sa_handler= SIG_IGN;
  if (sigemptyset(&sa.sa_mask) == -1 ||
      sigaction(SIGPIPE, &sa, 0) == -1) {
    std::cerr << "Could not set SIGPIPE handler.";
    return true;
  }

  sa.sa_handler = shutdown_handler;
  if ((sigaction(SIGTERM, &sa, 0) == -1) ||
      (sigaction(SIGINT, &sa, 0) == -1)  ||
      (sigaction(SIGUSR1, &sa, 0) == -1) ||
      (sigaction(SIGHUP, &sa, 0) == -1)) {
    std::cerr << "Could not set SIGTERM|SIGINT|SIGUSR1|SIGHUP handler." << std::endl;
    return true;
  }

  return false;
}

static void thread_delegate(std::mutex& mutex,
                            std::condition_variable& cv,
                            bool& thread_started,
                            ThreadRegistryPtr registry,
                            std::string pool,
                            StringSet servers_list,
                            int64_t timeout,
                            StringSet jobs_list,
                            std::string http_uri) noexcept {
    std::unique_lock<std::mutex> lock(mutex);
    ThreadLoop loop(registry, pool, servers_list, timeout, jobs_list, http_uri);
    thread_started = true;
    lock.unlock();
    cv.notify_one();

    // mutex, cv and thread_started are not safe to use after the notify!
    return loop.run(0);
}

void MainLoop::modifyPool(const std::string& name) noexcept {
    const auto iter = m_config.m_pool_map.find(name);

    if (iter == m_config.m_pool_map.end()) {
        LOG4CXX_ERROR(MainLogger, "modifyPool requested, " << name << " does not exist in config");
        throw std::runtime_error("invalid config detected in modifyPool");
    }

    const auto& pooldata = iter->second;
    uint32_t running_count = m_thread_registry->poolCount(name);

    if (running_count > pooldata.first) {
        LOG4CXX_INFO(MainLogger, "Currently running: " << running_count << ". Decrease to: " << pooldata.first);
        m_thread_registry->sendShutdown(name, running_count - pooldata.first);
    } else if (running_count < pooldata.first) {
        LOG4CXX_INFO(MainLogger, "Currently running: " << running_count << ". Increase to: " << pooldata.first);
        for (uint32_t i = (pooldata.first - running_count); i > 0; i--) {
            std::mutex mutex;
            std::condition_variable cv;
            bool thread_started = false;

            std::thread t(thread_delegate,
                          std::ref(mutex),
                          std::ref(cv),
                          std::ref(thread_started),
                          m_thread_registry,
                          name,
                          m_config.m_servers_list,
                          m_config.m_timeout,
                          pooldata.second,
                          m_config.m_http_uri);

            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&thread_started]{return thread_started;});
            t.detach();
        }
    }
}

bool MainLoop::loadConfig(DriveshaftConfig& new_config) noexcept {
    struct stat sb;
    if (stat(m_config_filename.c_str(), &sb) == -1) {
        char buffer[1024];
        strerror_r(errno, buffer, 1024);
        LOG4CXX_ERROR(MainLogger, "Unable to stat file " << m_config_filename << ". Error: " << buffer);
        throw std::runtime_error("stat failure");
    }

    if (sb.st_mtime > m_config.m_load_time) {
        LOG4CXX_INFO(MainLogger, "Reloading config");
        namespace pt = boost::property_tree;
        pt::ptree tree;
        try {
            pt::read_json(m_config_filename, tree);

            new_config.m_load_time = sb.st_mtime;
            new_config.m_servers_list.clear();
            new_config.m_timeout = tree.get<uint32_t>("server_timeout");
            new_config.m_http_uri = tree.get<std::string>("http_uri");
            new_config.m_pool_map.clear();

            LOG4CXX_DEBUG(MainLogger, "New config: URI=" << new_config.m_http_uri <<
                          " timeout=" << new_config.m_timeout);

            for (auto&v : tree.get_child("servers_list")) {
                std::string name = v.second.get_value<std::string>();
                LOG4CXX_DEBUG(MainLogger, "Read server: " << name);
                new_config.m_servers_list.insert(name);
            }

            for (auto& v : tree.get_child("independent_workers")) {
                std::string name = v.first;
                uint32_t count = v.second.get_value<uint32_t>(0);
                LOG4CXX_DEBUG(MainLogger, "Read independent member: " << name << " with count " << count);
                auto& pooldata = new_config.m_pool_map[name];
                pooldata.first = count;
                pooldata.second.insert(name);
            }

            auto& pooldata = new_config.m_pool_map[""]; // Global "All" pool
            pooldata.first = tree.get<uint32_t>("worker_count");
            for (auto& v : tree.get_child("regular_workers")) {
                std::string name = v.second.get_value<std::string>();
                LOG4CXX_DEBUG(MainLogger, "Read regular pool member: " << name);
                pooldata.second.insert(name);
            }

            return true;
        } catch (std::exception& e) {
            LOG4CXX_ERROR(MainLogger, "Error parsing config " << e.what());
            throw;
        }
    }

    return false;
}

std::pair<StringSet, StringSet> DriveshaftConfig::compare(const DriveshaftConfig& new_config) const noexcept {
    LOG4CXX_DEBUG(MainLogger, "Beginning config compare");

    StringSet current_pool_names, latest_pool_names;
    boost::copy(m_pool_map | boost::adaptors::map_keys,
                std::inserter(current_pool_names, current_pool_names.begin()));
    boost::copy(new_config.m_pool_map | boost::adaptors::map_keys,
                std::inserter(latest_pool_names, latest_pool_names.begin()));

    if ((m_http_uri != new_config.m_http_uri) ||
        (m_servers_list != new_config.m_servers_list) ||
        (m_timeout != new_config.m_timeout)) {
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

        // Check for changed jobs
        for (const auto& i : m_pool_map) {
            auto found = new_config.m_pool_map.find(i.first);
            if (found != new_config.m_pool_map.end()) {
                const auto& lhs_jobs_set = i.second.second;
                const auto& rhs_jobs_set = found->second.second;
                StringSet diff;
                std::set_symmetric_difference(lhs_jobs_set.begin(), lhs_jobs_set.end(),
                                              rhs_jobs_set.begin(), rhs_jobs_set.end(),
                                              std::inserter(diff, diff.begin()));

                if (diff.size()) {
                    pools_turn_off.insert(i.first);
                    pools_turn_on.insert(i.first);
                }
            }
        }

        return std::pair<StringSet, StringSet>(std::move(pools_turn_off), std::move(pools_turn_on));
    }
}

}
