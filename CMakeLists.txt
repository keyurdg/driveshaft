cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

# includes
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})

# package information
set(PACKAGE_NAME      "driveshaft")
set(PACKAGE_VERSION   "0.2.0")
set(PACKAGE_STRING    "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_TARNAME   "${PACKAGE_NAME}-${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "https://github.com/keyurdg/driveshaft/issues")

# for man page
set(MAN_INSTALL_DIR share/man/man1)

project(${PACKAGE_NAME} C CXX ASM)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
    message(STATUS "Build type not specified: cmake build type RelWithDebInfo.")
endif()

include(Compiler)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
include(FindThreads)
list(APPEND DRIVESHAFT_LINK_LIBRARIES Threads::Threads)
include(FindPrometheus)
include(FindCurl)
include(FindGearman)
include(GetBoost)
include(FindLog4cxx)
include(CheckFunctionExists)
include(GNUInstallDirs) # use GNU install dirs (e.g. lib64 instead of lib)

mark_as_advanced(CLEAR CMAKE_INSTALL_PREFIX)

## Uncomment the below to disable optimizations for debugging
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

add_subdirectory(src bin/)

enable_testing()
add_subdirectory(tests bin/test/)

file(WRITE src/version.h
  "\#ifndef DRIVESHAFT_VERSION\n\#define DRIVESHAFT_VERSION \"@PACKAGE_VERSION@\"\n\#endif\n"
)

add_custom_command(OUTPUT driveshaft.1
  COMMAND txt2man -t "driveshaft" -s 1 -v "User Manual" driveshaft.1.txt > driveshaft.1
  DEPENDS driveshaft.1.txt)

add_custom_target (manpage ALL DEPENDS driveshaft.1)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/driveshaft.1 DESTINATION ${CMAKE_INSTALL_PREFIX}/${MAN_INSTALL_DIR})
