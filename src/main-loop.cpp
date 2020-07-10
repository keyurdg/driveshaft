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
#include <cstring>
#include <unistd.h>
#include <errno.h>
#include <fstream>
#include <boost/asio.hpp>
#include <exception>
#include <utility>
#include <mutex>
#include <condition_variable>
#include "common-defs.h"
#include "thread-registry.h"
#include "metric-proxy.h"
#include "main-loop.h"
#include "thread-loop.h"
#include "gearman-client.h"

namespace Driveshaft {

static std::mutex s_new_thread_mutex;
static std::condition_variable s_new_thread_cond;
static enum class ThreadStartState {
    INIT,
    SUCCESS,
    FAILURE
} s_new_thread_wakeup = ThreadStartState::INIT;

static void gearman_thread_delegate(ThreadRegistryPtr registry,
                            MetricProxyPtr metrics,
                            std::string pool,
                            StringSet servers_list,
                            StringSet jobs_list,
                            std::string http_uri) noexcept {
    std::unique_lock<std::mutex> lock(s_new_thread_mutex);
    const MetricProxyPoolWrapperPtr metricsPoolWrapper = MetricProxyPoolWrapper::wrap(pool, metrics);
    GearmanClient *client = new GearmanClient(registry, metricsPoolWrapper, servers_list,
                                              jobs_list, http_uri);
    ThreadLoop loop(registry, pool, client);
    s_new_thread_wakeup = ThreadStartState::SUCCESS;
    lock.unlock();
    s_new_thread_cond.notify_one();

    return loop.run(0);
}

class ThreadPoolWatcher : public PoolWatcher {
public:
    ThreadPoolWatcher(ThreadRegistryPtr registry, MetricProxyPtr metrics) :
         m_thread_registry(registry)
        ,m_metrics_proxy(metrics) {
    }

    virtual void inform(uint32_t config_worker_count, const std::string& pool_name,
                        const StringSet& server_list, const StringSet& jobs_list,
                        const std::string& processing_uri) {
        uint32_t current_worker_count = m_thread_registry->poolCount(pool_name);
        if (current_worker_count > config_worker_count) {
            uint32_t num_workers_to_stop = current_worker_count - config_worker_count;
            LOG4CXX_INFO(MainLogger, "stopping " << num_workers_to_stop << " threads");
            m_thread_registry->sendShutdown(pool_name, num_workers_to_stop);
        } else if (current_worker_count < config_worker_count) {
            uint32_t num_workers_to_start = config_worker_count - current_worker_count;
            LOG4CXX_INFO(MainLogger, "starting " << num_workers_to_start << " threads");
            for (uint32_t i = num_workers_to_start; i > 0; i--) {
                std::unique_lock<std::mutex> lock(s_new_thread_mutex);
                s_new_thread_wakeup = ThreadStartState::INIT;

                std::thread t(gearman_thread_delegate,
                              m_thread_registry,
                              m_metrics_proxy,
                              pool_name, server_list,
                              jobs_list, processing_uri);

                s_new_thread_cond.wait(lock, []{return s_new_thread_wakeup != ThreadStartState::INIT;});
                t.detach();
            }
        }
    }

private:
    ThreadRegistryPtr m_thread_registry;
    MetricProxyPtr m_metrics_proxy;
};

MainLoop::MainLoop(const std::string& config_file) :
    m_config_filename(config_file),
    m_config(),
    m_thread_registry(new ThreadRegistry),
    m_metric_proxy(new MetricProxy("0.0.0.0:8888")),
    m_pool_watcher(new ThreadPoolWatcher(m_thread_registry, m_metric_proxy)) {
    if (!setupSignals()) {
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

void MainLoop::doShutdown(uint32_t wait) noexcept {
    g_force_shutdown = true;
    this->m_config.clearAllWorkerCounts(*m_pool_watcher);
    std::this_thread::sleep_for(std::chrono::seconds(wait));
}

void MainLoop::run() {
    Json::CharReaderBuilder jsonfactory;
    jsonfactory.strictMode(&jsonfactory.settings_);
    std::shared_ptr<Json::CharReader> json_parser(jsonfactory.newCharReader());

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
        new_config.load(this->m_config_filename, json_parser);

        new_config.supersede(m_config, *m_pool_watcher);
        m_config = new_config;

        std::this_thread::sleep_for(std::chrono::seconds(LOOP_SLEEP_DURATION));
    }
}

bool MainLoop::setupSignals() const noexcept {
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));

    sa.sa_handler = SIG_IGN;
    if (sigemptyset(&sa.sa_mask) == -1 ||
        sigaction(SIGPIPE, &sa, 0) == -1) {
        LOG4CXX_ERROR(MainLogger, "Could not set SIGPIPE handler");
        return false;
    }

    sa.sa_handler = shutdown_handler;
    if ((sigaction(SIGTERM, &sa, 0) == -1) ||
        (sigaction(SIGINT, &sa, 0) == -1)  ||
        (sigaction(SIGUSR1, &sa, 0) == -1) ||
        (sigaction(SIGHUP, &sa, 0) == -1)) {
        LOG4CXX_ERROR(MainLogger, "Could not set SIGTERM|SIGINT|SIGUSR1|SIGHUP handler");
        return false;
    }

    return true;
}

} // namespace Driveshaft
