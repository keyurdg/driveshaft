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

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <string>
#include <iostream>
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
#include "pidfile.h"

namespace Driveshaft {

static const char* MAIN_LOGGER_NAME = "Main";
static const char* THREAD_LOGGER_NAME = "Thread";

/* Define the externs from common-defs */
std::atomic_bool g_force_shutdown(false);
log4cxx::LoggerPtr MainLogger(log4cxx::Logger::getLogger(MAIN_LOGGER_NAME));
log4cxx::LoggerPtr ThreadLogger(log4cxx::Logger::getLogger(THREAD_LOGGER_NAME));
uint32_t MAX_JOB_RUNNING_TIME;
uint32_t GEARMAND_RESPONSE_TIMEOUT; // This drives all the other timeouts below
uint32_t LOOP_SLEEP_DURATION;
uint32_t HARD_SHUTDOWN_WAIT_DURATION;
uint32_t GRACEFUL_SHUTDOWN_WAIT_DURATION;

}

/* true on success, false on failure */
static bool switch_user(const std::string& username) {
    if (getuid() == 0 || geteuid() == 0) {
        struct passwd *pw = getpwnam(username.c_str());
        if (!pw) {
            std::cout << "Could not find user: " << username << std::endl;
            return false;
        }

        if (setgid(pw->pw_gid) == -1 || setuid(pw->pw_uid) == -1) {
            std::cout << "Could not switch to user: " << username << std::endl;
            return false;
        }
    } else {
        std::cout << "Must be root to switch users" << std::endl;
        return false;
    }

    return true;
}

static bool do_daemon() {
    pid_t child = fork();

    switch (child) {
    case -1:
        /* strerror is OK because we're still in a single threaded context */
        std::cout << "Failed to fork child. errno: " << errno << ". Error details: " << strerror(errno) << std::endl;
        return false;

    case 0:
        /* child */
        break;

    default:
        /* parent */
        exit(0);
    }

    if (setsid() == -1) {
        std::cout << "Failed to setsid in child process: errno: " << errno << ". Error details: " << strerror(errno) << std::endl;
        return false;
    }

    if (chdir("/") < 0) {
        std::cout << "Failed to chdir to / in child process: errno: " << errno << ". Error details: " << strerror(errno) << std::endl;
        return false;
    }

    return true;
}

int main(int argc, char **argv) {
    int rc;
    std::string jobs_config_file;
    std::string log_config_file;
    std::string username;
    std::string pid_filename;
    bool daemonize = false;

    /* Parse command line opts */
    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "produce help message")
            ("version", "print version string")
            ("user", po::value<std::string>(&username), "username to run as")
            ("pid_file", po::value<std::string>(&pid_filename), "file to write process ID out to")
            ("daemonize", po::bool_switch(&daemonize)->default_value(false), "Daemon, detach and run in the background")
            ("jobsconfig", po::value<std::string>(&jobs_config_file)->required(), "jobs config file path")
            ("logconfig", po::value<std::string>(&log_config_file)->required(), "log config file path")
            ("max_running_time", po::value<uint32_t>(&Driveshaft::MAX_JOB_RUNNING_TIME)->required(), "how long can a job run before it is considered failed (in seconds)")
            ("loop_timeout", po::value<uint32_t>(&Driveshaft::GEARMAND_RESPONSE_TIMEOUT)->required(), "how long to wait for a response from gearmand before restarting event-loop (in seconds)")
    ;

    try {
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("version")) {
            std::cout << "driveshaft version: " DRIVESHAFT_VERSION << std::endl;
            return 1;
        }

        if (vm.empty() || vm.count("help")) {
            std::cout << desc << std::endl;
            return 1;
        }

        po::notify(vm);
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
        std::cout << desc << std::endl;
        return 1;
    }

    Driveshaft::LOOP_SLEEP_DURATION = (uint32_t) (Driveshaft::GEARMAND_RESPONSE_TIMEOUT / 2);
    Driveshaft::HARD_SHUTDOWN_WAIT_DURATION = Driveshaft::GEARMAND_RESPONSE_TIMEOUT * 2;
    Driveshaft::GRACEFUL_SHUTDOWN_WAIT_DURATION = Driveshaft::HARD_SHUTDOWN_WAIT_DURATION * 2;

    if ((username.length() > 0) && (switch_user(username) == false)) {
        return 1;
    }

    if (daemonize && (do_daemon() == false)) {
        return 1;
    }

    // RAII: On going out of scope will clean up the file
    datadifferential::util::Pidfile pid_file(pid_filename);
    if ((pid_filename.length() > 0) && (pid_file.create() == false)) {
        std::cout << "Unable to write pidfile due to errors: " << pid_file.error_message() << std::endl;
        return 1;
    }

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
    curl_global_init(CURL_GLOBAL_ALL);

    /* Enter main loop */
    rc = 0;
    try {
        LOG4CXX_INFO(Driveshaft::MainLogger, "Starting up with gearmand response timeout=" << Driveshaft::GEARMAND_RESPONSE_TIMEOUT
                                             << " and max running time=" << Driveshaft::MAX_JOB_RUNNING_TIME);

        Driveshaft::MainLoop loop(jobs_config_file);
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
