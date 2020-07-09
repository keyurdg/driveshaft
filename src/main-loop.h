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

#ifndef incl_DRIVESHAFT_MAIN_LOOP_H_
#define incl_DRIVESHAFT_MAIN_LOOP_H_

#include <string>
#include <map>
#include <utility>
#include <time.h>
#include "common-defs.h"
#include "thread-registry.h"
#include "driveshaft-config.h"

namespace Driveshaft {

class MainLoop {
public:
    // treat this class as a singleton. you really, really don't want more
    // than one in your process.
    MainLoop(const std::string& config_file);
    void run();
    ~MainLoop() = default;

private:
    bool setupSignals() const noexcept;
    void doShutdown(uint32_t wait) noexcept;

    MainLoop() = delete;
    MainLoop(const MainLoop&) = delete;
    MainLoop(MainLoop&&) = delete;
    MainLoop& operator=(const MainLoop&) = delete;
    MainLoop& operator=(const MainLoop&&) = delete;

    std::string m_config_filename;
    DriveshaftConfig m_config;
    ThreadRegistryPtr m_thread_registry;
    std::shared_ptr<PoolWatcher> m_pool_watcher;
};

} // namespace Driveshaft

#endif // incl_DRIVESHAFT_MAIN_REGISTRY_H_
