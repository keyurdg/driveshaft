CMAKE_MINIMUM_REQUIRED(VERSION 2.8.7 FATAL_ERROR)

# includes
SET(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})

# package information
SET(PACKAGE_NAME      "driveshaft")
SET(PACKAGE_VERSION   "0.1.0")
SET(PACKAGE_STRING    "${PACKAGE_NAME} ${PACKAGE_VERSION}")
SET(PACKAGE_TARNAME   "${PACKAGE_NAME}-${PACKAGE_VERSION}")
SET(PACKAGE_BUGREPORT "https://github.com/keyurdg/driveshaft/issues")

PROJECT(${PACKAGE_NAME} C CXX ASM)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
    message(STATUS "Build type not specified: cmake build type RelWithDebInfo.")
endif()

include(Compiler)
include(FindCurl)
include(FindGearman)
include(FindPThread)
include(GetBoost)
include(FindLog4cxx)

MARK_AS_ADVANCED(CLEAR CMAKE_INSTALL_PREFIX)

INCLUDE(CheckFunctionExists)

FILE(WRITE src/version.h
  "\#ifndef DRIVESHAFT_VERSION\n\#define DRIVESHAFT_VERSION \"@PACKAGE_VERSION@\"\n\#endif\n"
  )

ADD_SUBDIRECTORY(tests/unit/gtest-1.7.0)
ADD_SUBDIRECTORY(src)
INCLUDE_DIRECTORIES(tests/unit/gtest-1.7.0/include ${COMMON_INCLUDES})

# use GNU install dirs (e.g. lib64 instead of lib)
INCLUDE(GNUInstallDirs)

enable_testing()
add_executable(driveshaft_unit_tests tests/unit/tests.cpp)
target_link_libraries(driveshaft_unit_tests gtest)
add_test(AllTestsInDriveShaft driveshaft_unit_tests)
include(AddIntegrationTest)
