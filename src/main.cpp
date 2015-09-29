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

#include <string>
#include <iostream>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/program_options.hpp>
#include <set>
#include <exception>
#include <utility>
#include <curl/curl.h>
#include <log4cxx/logger.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/xml/domconfigurator.h>
#include <log4cxx/helpers/exception.h>
#include "common-defs.h"
#include "main-loop.h"
#include "version.h"

namespace Driveshaft {

static const char* MAIN_LOGGER_NAME = "Main";
static const char* THREAD_LOGGER_NAME = "Thread";
static const char* STATUS_LOGGER_NAME = "Status";

/* Define the externs from common-defs */
std::atomic_bool g_force_shutdown;
log4cxx::LoggerPtr MainLogger(log4cxx::Logger::getLogger(MAIN_LOGGER_NAME));
log4cxx::LoggerPtr ThreadLogger(log4cxx::Logger::getLogger(THREAD_LOGGER_NAME));
log4cxx::LoggerPtr StatusLogger(log4cxx::Logger::getLogger(STATUS_LOGGER_NAME));
uint32_t STATUS_PORT;
uint32_t MAX_JOB_RUNNING_TIME;
uint32_t GEARMAND_RESPONSE_TIMEOUT; // This drives all the other timeouts below
uint32_t LOOP_SLEEP_DURATION;
uint32_t HARD_SHUTDOWN_WAIT_DURATION;
uint32_t GRACEFUL_SHUTDOWN_WAIT_DURATION;

}

int main(int argc, char **argv) {
    int rc;
    std::string jobs_config_file;
    std::string log_config_file;

    /* Parse command line opts */
    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "produce help message")
            ("version", "print version string")
            ("jobsconfig", po::value<std::string>(&jobs_config_file), "jobs config file path")
            ("logconfig", po::value<std::string>(&log_config_file), "log config file path")
            ("max_running_time", po::value<uint32_t>(&Driveshaft::MAX_JOB_RUNNING_TIME), "how long can a job run before it is considered failed (in seconds)")
            ("loop_timeout", po::value<uint32_t>(&Driveshaft::GEARMAND_RESPONSE_TIMEOUT), "how long to wait for a response from gearmand before restarting event-loop (in seconds)")
            ("status_port", po::value<uint32_t>(&Driveshaft::STATUS_PORT), "port to listen on to return status")
    ;

    try {
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
        if (vm.count("version")) {
            std::cout << "driveshaft version: " DRIVESHAFT_VERSION << std::endl;
            return 1;
        }
        if (vm.empty()
            || vm.count("help")
            || !vm.count("jobsconfig")
            || !vm.count("logconfig")
            || !vm.count("max_running_time")
            || !vm.count("status_port")
            || !vm.count("loop_timeout")) {
            std::cout << desc << std::endl;
            return 1;
        }
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
        std::cout << desc << std::endl;
        return 1;
    }

    Driveshaft::LOOP_SLEEP_DURATION = (uint32_t) (Driveshaft::GEARMAND_RESPONSE_TIMEOUT / 2);
    Driveshaft::HARD_SHUTDOWN_WAIT_DURATION = Driveshaft::GEARMAND_RESPONSE_TIMEOUT * 2;
    Driveshaft::GRACEFUL_SHUTDOWN_WAIT_DURATION = Driveshaft::HARD_SHUTDOWN_WAIT_DURATION * 2;

    /* Load log config */
    try {
        /* ::configure may segfault on log4cxx <= 0.10 if log_config_file is
           invalid. This is fixed in log4cxx trunk. */
        log4cxx::xml::DOMConfigurator::configure(log_config_file);
    } catch (log4cxx::helpers::Exception& e) {
        std::cout << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "Unable to load log-config " << log_config_file << std::endl;
        return 1;
    }

    /* Global init */
    Driveshaft::g_force_shutdown = false;
    curl_global_init(CURL_GLOBAL_ALL);

    /* Enter main loop */
    rc = 0;
    try {
        LOG4CXX_INFO(Driveshaft::MainLogger, "Starting up with gearmand response timeout=" << Driveshaft::GEARMAND_RESPONSE_TIMEOUT
                                             << " and max running time=" << Driveshaft::MAX_JOB_RUNNING_TIME);

        Driveshaft::MainLoopImpl loop(jobs_config_file);
        loop.run();
    } catch (std::exception& e) {
        std::cout << "MainLoop threw exception: " << e.what() << std::endl;
        rc = 1;
    } catch (...) {
        std::cout << "MainLoop threw exception" << std::endl;
        rc = 1;
    }

    /* Global cleanup */
    curl_global_cleanup();
    return rc;
}
