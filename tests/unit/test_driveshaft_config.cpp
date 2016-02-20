#include "gtest/gtest.h"
#include "driveshaft-config.h"
#include "./data/configs.h"

using namespace Driveshaft;

class TestPoolWatcher : public PoolWatcher {
public:
    // list of callbacks received
    std::vector<std::pair<std::string, uint32_t>> callbacksSeen;

    // map of pool -> most recent worker count
    std::map<std::string, uint32_t> poolsCleared;

    TestPoolWatcher() : poolsCleared() {}

    virtual void inform(uint32_t configWorkerCount, const std::string &poolName,
                        const StringSet &serverList, const StringSet &jobsList,
                        const std::string &processingUri) {
        auto pair(std::make_pair(poolName, configWorkerCount));
        this->poolsCleared.emplace(pair);
        callbacksSeen.push_back(pair);
    }
};

class DriveshaftConfigTest : public ::testing::Test {
public:
    TestPoolWatcher watcher;
    Json::CharReaderBuilder jsonFactory;
};

TEST_F(DriveshaftConfigTest, TestClearWorkerCount) {
    DriveshaftConfig config;
    config.parseConfig(testConfigOneServerOnePool, jsonFactory);
    std::string poolToClear("test-pool-1");
    config.clearWorkerCount(poolToClear, watcher);

    ASSERT_EQ(1, watcher.poolsCleared.size());
    ASSERT_NE(watcher.poolsCleared.end(), watcher.poolsCleared.find(poolToClear));
    ASSERT_EQ(0, watcher.poolsCleared[poolToClear]);
}

TEST_F(DriveshaftConfigTest, TestClearAllWorkerCounts) {
    DriveshaftConfig config;
    config.parseConfig(testConfigOneServerTwoPools, jsonFactory);

    TestPoolWatcher watcher;
    config.clearAllWorkerCounts(watcher);

    ASSERT_EQ(2, watcher.poolsCleared.size());

    std::vector<std::string> poolsCleared { "test-pool-1", "test-pool-2" };
    for (const auto &poolName : poolsCleared) {
        ASSERT_NE(watcher.poolsCleared.end(), watcher.poolsCleared.find(poolName));
        ASSERT_EQ(0, watcher.poolsCleared[poolName]);
    }
}

TEST_F(DriveshaftConfigTest, TestCompareInvalidatesAllPoolsOnServerListChange) {
    DriveshaftConfig oldconf, newconf;
    oldconf.parseConfig(testConfigOneServerTwoPools, jsonFactory);
    newconf.parseConfig(testConfigTwoServersTwoPools, jsonFactory);

    StringSet toRemove, toAdd;
    std::tie(toRemove, toAdd) = oldconf.compare(newconf);

    ASSERT_EQ(2, toRemove.size());
    ASSERT_EQ(2, toAdd.size());
    std::vector<std::string> poolsCleared { "test-pool-1", "test-pool-2" };
    for (const auto &poolName : poolsCleared) {
        // it just so happens that the pools are the same in this test
        ASSERT_NE(toRemove.end(), toRemove.find(poolName));
        ASSERT_NE(toAdd.end(), toAdd.find(poolName));
    }
}

TEST_F(DriveshaftConfigTest, TestCompareAddsPoolOnConfigChange) {
    DriveshaftConfig oldconf, newconf;
    oldconf.parseConfig(testConfigOneServerOnePool, jsonFactory);
    newconf.parseConfig(testConfigOneServerTwoPools, jsonFactory);

    StringSet toRemove, toAdd;
    std::tie(toRemove, toAdd) = oldconf.compare(newconf);

    ASSERT_EQ(0, toRemove.size());
    ASSERT_EQ(1, toAdd.size());
    std::string poolAdded("test-pool-2");
    ASSERT_NE(toAdd.end(), toAdd.find(poolAdded));
}

TEST_F(DriveshaftConfigTest, TestCompareInvalidateRemovesPoolOnConfigChange) {
    DriveshaftConfig oldconf, newconf;
    oldconf.parseConfig(testConfigOneServerTwoPools, jsonFactory);
    newconf.parseConfig(testConfigOneServerOnePool, jsonFactory);

    StringSet toRemove, toAdd;
    std::tie(toRemove, toAdd) = oldconf.compare(newconf);

    ASSERT_EQ(1, toRemove.size());
    ASSERT_EQ(0, toAdd.size());
    std::string poolRemoved("test-pool-2");
    ASSERT_NE(toRemove.end(), toRemove.find(poolRemoved));
}

TEST_F(DriveshaftConfigTest, TestCompareInvalidatesOnProcessingUriChange) {
    DriveshaftConfig oldconf, newconf;
    oldconf.parseConfig(testConfigOneServerOnePool, jsonFactory);
    newconf.parseConfig(testConfigOneServerOnePoolNewUri, jsonFactory);

    StringSet toRemove, toAdd;
    std::tie(toRemove, toAdd) = oldconf.compare(newconf);

    ASSERT_EQ(1, toRemove.size());
    ASSERT_EQ(1, toAdd.size());
    std::string poolRemoved("test-pool-1");
    std::string poolAdded("test-pool-1");
    ASSERT_NE(toRemove.end(), toRemove.find(poolRemoved));
    ASSERT_NE(toAdd.end(), toAdd.find(poolAdded));
}

TEST_F(DriveshaftConfigTest, TestSupersedeNotifiesPoolWatcher) {
    DriveshaftConfig oldconf, newconf;
    oldconf.parseConfig(testConfigOneServerOnePool, jsonFactory);
    newconf.parseConfig(testConfigOneServerOnePoolNewUri, jsonFactory);

    TestPoolWatcher watcher;
    newconf.supersede(oldconf, watcher);


    std::string poolRemoved("test-pool-1");
    std::string poolAdded("test-pool-1");
    ASSERT_EQ(2, watcher.callbacksSeen.size());
    ASSERT_EQ(std::make_pair(poolRemoved, uint32_t(0)), watcher.callbacksSeen[0]);
    ASSERT_EQ(std::make_pair(poolAdded, uint32_t(5)), watcher.callbacksSeen[1]);
}
