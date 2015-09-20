FIND_PATH(GEARMAN_INCLUDE_DIR NAMES libgearman-1.0/gearman.h libgearman-1.0/interface/status.h libgearman-1.0/status.h libgearman-1.0/worker.h
     PATHS /usr/include /usr/local/include)

FIND_LIBRARY(GEARMAN_LIBRARY NAMES gearman PATHS /usr/lib64 /usr/local/lib64)

IF (GEARMAN_INCLUDE_DIR AND GEARMAN_LIBRARY)
  MESSAGE(STATUS "Gearman Include dir: ${GEARMAN_INCLUDE_DIR}")
  MESSAGE(STATUS "libgearman library: ${GEARMAN_LIBRARY}")
ELSE()
  MESSAGE(FATAL_ERROR "Cannot find libgearman library")
ENDIF()

include_directories(${GEARMAN_INCLUDE_DIR})
LIST(APPEND DRIVESHAFT_LINK_LIBRARIES gearman)
