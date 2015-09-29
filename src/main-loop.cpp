/*
 * Driveshaft: Gearman worker manager
 *
 * Copyright (c) [2015] [Keyur Govande]
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <iostream>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fstream>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/asio.hpp>
#include <exception>
#include <utility>
#include <mutex>
#include <condition_variable>
#include "common-defs.h"
#include "thread-registry.h"
#include "main-loop.h"
#include "thread-loop.h"
#include "status-loop.h"

namespace Driveshaft {

MainLoop::MainLoop(const std::string& config_file) noexcept
    : m_config_filename(config_file)
    , m_config()
    , m_thread_registry(new ThreadRegistry())
    , m_json_parser(nullptr) {
    if (setupSignals()) {
        throw std::runtime_error("Unable to setup signals");
    }

    Json::CharReaderBuilder jsonfactory;
    jsonfactory.strictMode(&jsonfactory.settings_);
    m_json_parser.reset(jsonfactory.newCharReader());
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

void MainLoop::doShutdown(uint32_t wait) noexcept {
    for (auto& i : m_config.m_pool_map) {
        i.second.worker_count = 0;
        modifyPool(i.first);
    }

    g_force_shutdown = true;
    std::this_thread::sleep_for(std::chrono::seconds(wait));
}

void MainLoop::run() {
    startStatusThread();

    while(true) {
        switch (shutdown_type) {
        case ShutdownType::GRACEFUL:
            LOG4CXX_INFO(MainLogger, "Shutting down gracefully...");
            return doShutdown(GRACEFUL_SHUTDOWN_WAIT_DURATION);

        case ShutdownType::HARD:
            LOG4CXX_INFO(MainLogger, "Shutting down hard...");
            return doShutdown(HARD_SHUTDOWN_WAIT_DURATION);

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
                    pooldata->second.worker_count = 0;
                    modifyPool(i);
                }
            }

            // pools to start will operate on the new config
            m_config = new_config;
        } else {
            LOG4CXX_DEBUG(MainLogger, "Config has not changed");
        }

        // Check the number of workers actually running matches the config
        for (auto const& i : m_config.m_pool_map) {
            modifyPool(i.first);
        }

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

static std::mutex s_new_thread_mutex;
static std::condition_variable s_new_thread_cond;
static enum class ThreadStartState {
    INIT,
    SUCCESS,
    FAILURE
} s_new_thread_wakeup = ThreadStartState::INIT;

static void gearman_thread_delegate(ThreadRegistryPtr registry,
                            std::string pool,
                            StringSet servers_list,
                            StringSet jobs_list,
                            std::string http_uri) noexcept {
    std::unique_lock<std::mutex> lock(s_new_thread_mutex);
    ThreadLoop loop(registry, pool, servers_list, jobs_list, http_uri);
    s_new_thread_wakeup = ThreadStartState::SUCCESS;
    lock.unlock();
    s_new_thread_cond.notify_one();

    return loop.run(0);
}

static void status_thread_delegate(ThreadRegistryPtr registry) {
    using boost::asio::ip::tcp;
    using namespace boost::asio;

    std::unique_lock<std::mutex> lock(s_new_thread_mutex);

    auto return_status_functor = [&lock](ThreadStartState status) {
        s_new_thread_wakeup = status;
        lock.unlock();
        s_new_thread_cond.notify_one();
    };

    try {
        io_service io_service;
        StatusLoop loop(io_service, registry);

        // this means we bound without issues
        // return success to caller and don't use them anymore
        return_status_functor(ThreadStartState::SUCCESS);

        io_service.run();
    } catch (std::exception& e) {
        LOG4CXX_ERROR(StatusLogger, "Unable to create status loop due to error: " << e.what());
        return_status_functor(ThreadStartState::FAILURE);
    }
}

void MainLoop::startStatusThread() {
    std::unique_lock<std::mutex> lock(s_new_thread_mutex);
    s_new_thread_wakeup = ThreadStartState::INIT;

    std::thread t(status_thread_delegate,
                  m_thread_registry);

    s_new_thread_cond.wait(lock, []{return s_new_thread_wakeup != ThreadStartState::INIT;});

    if (s_new_thread_wakeup == ThreadStartState::SUCCESS) {
        t.detach();
    } else {
        t.join();
        throw std::runtime_error("cannot start status thread");
    }

}

void MainLoop::modifyPool(const std::string& name) noexcept {
    const auto iter = m_config.m_pool_map.find(name);

    if (iter == m_config.m_pool_map.end()) {
        LOG4CXX_ERROR(MainLogger, "modifyPool requested, " << name << " does not exist in config");
        throw std::runtime_error("invalid config detected in modifyPool");
    }

    const auto& pooldata = iter->second;
    uint32_t running_count = m_thread_registry->poolCount(name);

    if (running_count > pooldata.worker_count) {
        LOG4CXX_INFO(MainLogger, "Currently running: " << running_count << ". Decrease to: " << pooldata.worker_count);
        m_thread_registry->sendShutdown(name, running_count - pooldata.worker_count);
    } else if (running_count < pooldata.worker_count) {
        LOG4CXX_INFO(MainLogger, "Currently running: " << running_count << ". Increase to: " << pooldata.worker_count);
        for (uint32_t i = (pooldata.worker_count - running_count); i > 0; i--) {
            std::unique_lock<std::mutex> lock(s_new_thread_mutex);
            s_new_thread_wakeup = ThreadStartState::INIT;

            std::thread t(gearman_thread_delegate,
                          m_thread_registry,
                          name,
                          m_config.m_servers_list,
                          pooldata.jobs_list,
                          pooldata.job_processing_uri);

            s_new_thread_cond.wait(lock, []{return s_new_thread_wakeup != ThreadStartState::INIT;});
            t.detach();
        }
    }
}

static const char* CONFIG_KEY_GEARMAN_SERVERS_LIST = "gearman_servers_list";
static const char* CONFIG_KEY_POOLS_LIST = "pools_list";
static const char* CONFIG_KEY_POOL_WORKER_COUNT = "worker_count";
static const char* CONFIG_KEY_POOL_JOBS_LIST = "jobs_list";
static const char* CONFIG_KEY_POOL_JOB_PROCESSING_URI = "job_processing_uri";

bool MainLoop::loadConfig(DriveshaftConfig& new_config) {
    struct stat sb;
    if (stat(m_config_filename.c_str(), &sb) == -1) {
        char buffer[1024];
        strerror_r(errno, buffer, 1024);
        LOG4CXX_ERROR(MainLogger, "Unable to stat file " << m_config_filename << ". Error: " << buffer);
        throw std::runtime_error("config stat failure");
    }

    if (sb.st_mtime > m_config.m_load_time) {
        LOG4CXX_INFO(MainLogger, "Reloading config");

        std::ifstream fd(m_config_filename);
        if (fd.fail()) {
            LOG4CXX_ERROR(MainLogger, "Unable to load file " << m_config_filename);
            throw std::runtime_error("config read failure");
        }

        Json::Value tree;
        std::string parse_errors;
        std::string file_data;
        file_data.append(std::istreambuf_iterator<char>(fd),
                         std::istreambuf_iterator<char>());

        if (!m_json_parser->parse(file_data.data(),
                                  file_data.data() + file_data.length(),
                                  &tree, &parse_errors)) {
            LOG4CXX_ERROR(MainLogger, "Failed to parse " << m_config_filename << ". Errors: " << parse_errors);
            throw std::runtime_error("config parse failure");
        }

        if (!tree.isMember(CONFIG_KEY_GEARMAN_SERVERS_LIST) ||
            !tree.isMember(CONFIG_KEY_POOLS_LIST) ||
            !tree[CONFIG_KEY_GEARMAN_SERVERS_LIST].isArray() ||
            !tree[CONFIG_KEY_POOLS_LIST].isObject()) {
            LOG4CXX_ERROR(MainLogger, "Config (" << m_config_filename << ") has one or more key malformed elements (" <<
                                      CONFIG_KEY_GEARMAN_SERVERS_LIST << ", " <<
                                      CONFIG_KEY_POOLS_LIST << ")");
            throw std::runtime_error("config parse failure");
        }

        new_config.m_load_time = sb.st_mtime;

        new_config.m_servers_list.clear();
        new_config.m_pool_map.clear();

        const auto& servers_list = tree[CONFIG_KEY_GEARMAN_SERVERS_LIST];
        for (auto i = servers_list.begin(); i != servers_list.end(); ++i) {
            if (!i->isString()) {
                LOG4CXX_ERROR(MainLogger, CONFIG_KEY_GEARMAN_SERVERS_LIST << " does not contain strings");
                throw std::runtime_error("config server list type failure");
            }

            const auto& name = i->asString();
            LOG4CXX_DEBUG(MainLogger, "Read server: " << name);
            new_config.m_servers_list.insert(name);
        }

        const auto& pools_list = tree[CONFIG_KEY_POOLS_LIST];
        for (auto i = pools_list.begin(); i != pools_list.end(); ++i) {
            const auto& pool_name = i.name();
            const auto& data = *i;

            if (!data.isMember(CONFIG_KEY_POOL_WORKER_COUNT) ||
                !data.isMember(CONFIG_KEY_POOL_JOBS_LIST) ||
                !data.isMember(CONFIG_KEY_POOL_JOB_PROCESSING_URI) ||
                !data[CONFIG_KEY_POOL_JOB_PROCESSING_URI].isString() ||
                !data[CONFIG_KEY_POOL_WORKER_COUNT].isUInt() ||
                !data[CONFIG_KEY_POOL_JOBS_LIST].isArray()) {
                LOG4CXX_ERROR(MainLogger, "Config (" << m_config_filename << ") has invalid " << CONFIG_KEY_POOLS_LIST <<
                                          " elements: (" <<
                                          CONFIG_KEY_POOL_WORKER_COUNT << ", " <<
                                          CONFIG_KEY_POOL_JOB_PROCESSING_URI << ", " <<
                                          CONFIG_KEY_POOL_JOBS_LIST << ")");
                throw std::runtime_error("config jobs list parse failure");
            }

            auto& pooldata = new_config.m_pool_map[pool_name];
            pooldata.worker_count = data[CONFIG_KEY_POOL_WORKER_COUNT].asUInt();
            pooldata.job_processing_uri = data[CONFIG_KEY_POOL_JOB_PROCESSING_URI].asString();

            LOG4CXX_DEBUG(MainLogger, "Read pool: " << pool_name << " with count " << pooldata.worker_count
                                      << " and URI " << pooldata.job_processing_uri);

            const auto& jobs_list = data[CONFIG_KEY_POOL_JOBS_LIST];

            for (auto j = jobs_list.begin(); j != jobs_list.end(); ++j) {
                if (!j->isString()) {
                    LOG4CXX_ERROR(MainLogger, CONFIG_KEY_POOL_JOBS_LIST << " does not contain strings");
                }

                const auto& name = j->asString();
                LOG4CXX_DEBUG(MainLogger, "Pool " << pool_name << " adding job " << name);
                pooldata.jobs_list.insert(name);
            }
        }

        return true;
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

    if (m_servers_list != new_config.m_servers_list) {
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
            auto found = new_config.m_pool_map.find(i.first);
            if (found != new_config.m_pool_map.end()) {
                bool should_restart = false;
                if (found->second.job_processing_uri != i.second.job_processing_uri) {
                    should_restart = true;
                } else {
                    const auto& lhs_jobs_set = i.second.jobs_list;
                    const auto& rhs_jobs_set = found->second.jobs_list;
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

}
