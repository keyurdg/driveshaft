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
