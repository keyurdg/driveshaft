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

#include "thread-loop.h"
#include "gearman-client.h"

namespace Driveshaft {

ThreadLoop::ThreadLoop(ThreadRegistryPtr registry,
                       const std::string& pool,
                       GearmanClient *client) noexcept :
                       m_registry(registry),
                       m_pool(pool),
                       m_client(client) {
    LOG4CXX_DEBUG(ThreadLogger, "Starting ThreadLoop for " << m_pool);
    std::thread::id tid(std::this_thread::get_id());
    m_registry->registerThread(m_pool, tid);
    m_registry->setThreadState(tid, std::string("Waiting for work"));
}

ThreadLoop::~ThreadLoop() noexcept {
    LOG4CXX_DEBUG(ThreadLogger, "Stopping ThreadLoop for " << m_pool);
    m_registry->unregisterThread(m_pool, std::this_thread::get_id());
}

static const int64_t MAX_THREAD_LOOP_ATTEMPTS = 5;
static const int64_t THREAD_LOOP_RETRY_SLEEP_DURATION = 10; // Seconds

void ThreadLoop::run(uint32_t attempts) noexcept {
    if (attempts > MAX_THREAD_LOOP_ATTEMPTS) {
        LOG4CXX_ERROR(ThreadLogger, "Exceeded max loop attempts");
        return;
    }

    try {
        while (true) {
            // First check if we should shutdown
            if (g_force_shutdown || m_registry->shouldShutdown(std::this_thread::get_id())) {
                LOG4CXX_INFO(ThreadLogger, "Shutdown requested. Exiting loop");
                break;
            }

            // Do real work
            m_client->run();
        }
    } catch (GearmanClientException& e) {
        LOG4CXX_ERROR(ThreadLogger, "Caught GearmanClientException: " << e.what() << ". Retriable: " << e.retriable());
        if (e.retriable()) {
            LOG4CXX_INFO(ThreadLogger, "Sleeping for " << THREAD_LOOP_RETRY_SLEEP_DURATION << " seconds before retrying");
            std::this_thread::sleep_for(std::chrono::seconds(THREAD_LOOP_RETRY_SLEEP_DURATION));
            return run(++attempts);
        }
    } catch (std::exception& e) {
        LOG4CXX_ERROR(ThreadLogger, "Caught unhandled exception in ThreadLoop::run. Details: " << e.what());
    }

    return;
}

} // namespace Driveshaft
