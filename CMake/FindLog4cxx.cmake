FIND_PATH(LOG4CXX_INCLUDE_DIR NAMES log4cxx/logger.h
     PATHS /usr/include /usr/local/include)

FIND_LIBRARY(LOG4CXX_LIBRARY NAMES log4cxx PATHS /usr/lib64 /usr/local/lib64)

IF (LOG4CXX_INCLUDE_DIR AND LOG4CXX_LIBRARY)
  MESSAGE(STATUS "Log4cxx Include dir: ${LOG4CXX_INCLUDE_DIR}")
  MESSAGE(STATUS "liblog4cxx library: ${LOG4CXX_LIBRARY}")
ELSE()
  MESSAGE(FATAL_ERROR "Cannot find liblog4cxx library")
ENDIF()

include_directories(${LOG4CXX_INCLUDE_DIR})
LIST(APPEND DRIVESHAFT_LINK_LIBRARIES log4cxx)
