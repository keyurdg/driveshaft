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
