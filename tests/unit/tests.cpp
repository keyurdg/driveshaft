/*
 * Driveshaft: Gearman worker manager
 *
 * Copyright (c) [2015] [Keyur Govande, Daniel Schauenberg]
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

#include <iostream>
#include "gtest/gtest.h"
#include <boost/program_options.hpp>
#include <log4cxx/logger.h>
#include <log4cxx/consoleappender.h>
#include <log4cxx/simplelayout.h>

namespace Driveshaft {
    // define a symbol for MainLogger
    log4cxx::LoggerPtr MainLogger(log4cxx::Logger::getLogger("testing"));
}

void configureLoggersForTesting(const boost::program_options::variables_map &options) {
    auto testLogger = log4cxx::Logger::getLogger("testing");
    log4cxx::LevelPtr currentLevel(testLogger->getLevel());
    log4cxx::LevelPtr targetLevel(log4cxx::Level::getOff());

    if (options.count("verbose")) {
        auto consoleAppender = new log4cxx::ConsoleAppender(
                log4cxx::LayoutPtr(new log4cxx::SimpleLayout())
                );

        testLogger->addAppender(consoleAppender);
        targetLevel = currentLevel;
    }

    testLogger->setLevel(targetLevel);
}

boost::program_options::variables_map parseCommandLine(int argc, char **argv) {
    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()("verbose,v", "dump log output to console");

    po::variables_map options;
    po::store(po::parse_command_line(argc, argv, desc), options);
    po::notify(options);
    return options;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    auto options(parseCommandLine(argc, argv));
    configureLoggersForTesting(options);
    return RUN_ALL_TESTS();
}
