cmake_minimum_required(VERSION 2.8.7 FATAL_ERROR)

# includes
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})

# package information
set(PACKAGE_NAME      "driveshaft")
set(PACKAGE_VERSION   "0.2.0")
set(PACKAGE_STRING    "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_TARNAME   "${PACKAGE_NAME}-${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "https://github.com/keyurdg/driveshaft/issues")

project(${PACKAGE_NAME} C CXX ASM)

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
include(FindSnyder)
include(CheckFunctionExists)
include(GNUInstallDirs) # use GNU install dirs (e.g. lib64 instead of lib)

mark_as_advanced(CLEAR CMAKE_INSTALL_PREFIX)

add_subdirectory(src bin/)

enable_testing()
add_subdirectory(tests bin/test/)

file(WRITE src/version.h
  "\#ifndef DRIVESHAFT_VERSION\n\#define DRIVESHAFT_VERSION \"@PACKAGE_VERSION@\"\n\#endif\n"
)

add_custom_command(OUTPUT driveshaft.1
  COMMAND txt2man -t "driveshaft" -s 1 -v "User Manual" driveshaft.1.txt > driveshaft.1
  DEPENDS driveshaft.1.txt)

add_custom_target (manpage DEPENDS driveshaft.1)
