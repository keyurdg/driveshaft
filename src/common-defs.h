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

#ifndef incl_DRIVESHAFT_COMMON_DEFS_H_
#define incl_DRIVESHAFT_COMMON_DEFS_H_

#include <string>
#include <set>
#include <atomic>
#include <log4cxx/logger.h>

namespace Driveshaft {

typedef std::set<std::string> StringSet;

extern std::atomic_bool g_force_shutdown;
extern log4cxx::LoggerPtr MainLogger;
extern log4cxx::LoggerPtr ThreadLogger;
extern log4cxx::LoggerPtr StatusLogger;

extern uint32_t STATUS_PORT;
extern uint32_t MAX_JOB_RUNNING_TIME; // Expressed in seconds

extern uint32_t GEARMAND_RESPONSE_TIMEOUT; // This drives all the other timeouts below. Expressed in seconds
extern uint32_t LOOP_SLEEP_DURATION; // 0.5*GEARMAND_RESPONSE_TIMEOUT
extern uint32_t HARD_SHUTDOWN_WAIT_DURATION; // 2*GEARMAND_RESPONSE_TIMEOUT
extern uint32_t GRACEFUL_SHUTDOWN_WAIT_DURATION; // 2*HARD_SHUTDOWN_WAIT_DURATION

}

#endif // incl_DRIVESHAFT_COMMON_DEFS_H_
