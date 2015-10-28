FIND_PATH(SNYDER_INCLUDE_DIR NAMES snyder/metrics_registry.h PATHS /usr/include /usr/local/include)

FIND_LIBRARY(SNYDER_LIBRARY NAMES snyder PATHS /usr/lib64 /usr/local/lib64)

IF (SNYDER_INCLUDE_DIR)
  MESSAGE(STATUS "Snyder Include dir: ${SNYDER_INCLUDE_DIR}")
ELSE()
  MESSAGE(FATAL_ERROR "Cannot find snyder includedir")
ENDIF()

IF (SNYDER_LIBRARY)
  MESSAGE(STATUS "libsnyder library: ${SNYDER_LIBRARY}")
ELSE()
  MESSAGE(FATAL_ERROR "Cannot find libsnyder library")
ENDIF()

include_directories(${SNYDER_INCLUDE_DIR})
LIST(APPEND DRIVESHAFT_LINK_LIBRARIES snyder)
