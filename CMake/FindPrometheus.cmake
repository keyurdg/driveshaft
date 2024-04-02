FIND_PATH(PROMETHEUS_INCLUDE_DIR NAMES prometheus/exposer.h PATHS /usr/include /usr/local/include)

FIND_LIBRARY(PROMETHEUS_PULL_LIBRARY NAMES prometheus-cpp-pull PATHS /usr/lib64 /usr/local/lib64)
FIND_LIBRARY(PROMETHEUS_CORE_LIBRARY NAMES prometheus-cpp-core PATHS /usr/lib64 /usr/local/lib64)

IF (PROMETHEUS_INCLUDE_DIR)
  MESSAGE(STATUS "Prometheus Include dir: ${PROMETHEUS_INCLUDE_DIR}")
ELSE()
  MESSAGE(FATAL_ERROR "Cannot find prometheus includedir")
ENDIF()

IF (PROMETHEUS_PULL_LIBRARY)
  MESSAGE(STATUS "libprometheus-pull library: ${PROMETHEUS_PULL_LIBRARY}")
ELSE()
  MESSAGE(FATAL_ERROR "Cannot find libprometheus-pull library")
ENDIF()

IF (PROMETHEUS_CORE_LIBRARY)
  MESSAGE(STATUS "libprometheus-core library: ${PROMETHEUS_CORE_LIBRARY}")
ELSE()
  MESSAGE(FATAL_ERROR "Cannot find libprometheus-core library")
ENDIF()


include_directories(${PROMETHEUS_INCLUDE_DIR})
LIST(APPEND DRIVESHAFT_LINK_LIBRARIES prometheus-cpp-pull prometheus-cpp-core)
