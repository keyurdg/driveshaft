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

namespace Driveshaft {

static const char* MAIN_LOGGER_NAME = "Main";
static const char* THREAD_LOGGER_NAME = "Thread";

/* Define the externs from common-defs */
std::atomic_bool g_force_shutdown;
log4cxx::LoggerPtr MainLogger(log4cxx::Logger::getLogger(MAIN_LOGGER_NAME));
log4cxx::LoggerPtr ThreadLogger(log4cxx::Logger::getLogger(THREAD_LOGGER_NAME));

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
            ("jobsconfig", po::value<std::string>(&jobs_config_file), "jobs config file path")
            ("logconfig", po::value<std::string>(&log_config_file), "log config file path")
    ;

    try {
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
        if (vm.empty() || vm.count("help") || !vm.count("jobsconfig") || !vm.count("logconfig")) {
            std::cout << desc << std::endl;
            return 1;
        }
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
        std::cout << desc << std::endl;
        return 1;
    }

    /* Load log config */
    try {
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
        Driveshaft::MainLoop::getInstance(jobs_config_file).run();
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
