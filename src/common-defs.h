#ifndef incl_DRIVESHAFT_COMMON_DEFS_H_
#define incl_DRIVESHAFT_COMMON_DEFS_H_

#include <string>
#include <set>
#include <memory>
#include <atomic>
#include <log4cxx/logger.h>

namespace Driveshaft {

typedef std::set<std::string> StringSet;

extern std::atomic_bool g_force_shutdown;
extern log4cxx::LoggerPtr MainLogger;
extern log4cxx::LoggerPtr ThreadLogger;

}

#endif // incl_DRIVESHAFT_COMMON_DEFS_H_
