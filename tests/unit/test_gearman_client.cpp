#include <string>
#include <stdexcept>
#include <functional>
#include "gtest/gtest.h"
#include "mock/libs/gearman.h"
#include "mock/libs/curl.h"
#include "mock/classes/mock-thread-registry.h"
#include "mock/classes/mock-metric-proxy.h"
#include "gearman-client.h"

using namespace Driveshaft;

class ConfigurableMockGearmanWorkerLib : public mock::libs::gearman::MockGearmanWorkerLib {
public:
    ConfigurableMockGearmanWorkerLib() :
        waitCalled(false),
        timesWorkCalled(0),
        timesWaitCalled(0),
        workFunction(nullptr),
        workReturn(GEARMAN_NO_JOBS),
        waitReturn(GEARMAN_NO_JOBS),
        serversReturn(GEARMAN_SUCCESS),
        jobsReturn(GEARMAN_SUCCESS),
        gearmanClient(nullptr) {}

    gearman_worker_st* create(gearman_worker_st *worker) {
        return reinterpret_cast<gearman_worker_st*>(1);
    }

    gearman_return_t addServers(gearman_worker_st *worker, const char *servers) {
        return this->serversReturn;
    }

    gearman_return_t addFunction(gearman_worker_st *worker,
                                 const char *functionName, uint32_t timeout,
                                 gearman_worker_fn *function, void *context) {
        return this->jobsReturn;
    }

    gearman_return_t work(gearman_worker_st *worker) {
        // mechanism to terminate the run loop by throwing a test-specific err
        if (this->timesWorkCalled++ > 1) {
            throw new std::out_of_range("terminate run loop");
        }

        if (this->workFunction && this->gearmanClient) {
            return this->workFunction(gearmanClient);
        }
        return this->workReturn;
    }

    gearman_return_t wait(gearman_worker_st *worker) {
        this->timesWaitCalled++;
        return this->waitReturn;
    }

    void configure(gearman_return_t work, gearman_return_t wait,
                   gearman_return_t servers, gearman_return_t jobs) {
        this->workFunction = nullptr;
        this->workReturn = work;
        this->waitReturn = wait;
        this->serversReturn = servers;
        this->jobsReturn = jobs;
    }

    void configure_with_work_function(gearman_return_t (*work)(GearmanClient *),
                             GearmanClient* client,
                             gearman_return_t wait,
                             gearman_return_t servers, gearman_return_t jobs) {
        this->workFunction = work;
        this->gearmanClient = client;
        this->workReturn = GEARMAN_NO_JOBS;
        this->waitReturn = wait;
        this->serversReturn = servers;
        this->jobsReturn = jobs;
    }

    void reset() {
        this->waitCalled = false;
        this->workFunction = nullptr;
        this->workReturn = GEARMAN_NO_JOBS;
        this->waitReturn = GEARMAN_NO_JOBS;
        this->serversReturn = GEARMAN_SUCCESS;
        this->jobsReturn = GEARMAN_SUCCESS;
        this->timesWorkCalled = 0;
        this->timesWaitCalled = 0;
        this->gearmanClient = nullptr;
    }

    bool waitCalled;
    uint32_t timesWorkCalled, timesWaitCalled;


private:
    gearman_return_t (*workFunction)(GearmanClient *);
    gearman_return_t workReturn;
    gearman_return_t waitReturn;
    gearman_return_t serversReturn;
    gearman_return_t jobsReturn;

    GearmanClient *gearmanClient;
};

class ConfigurableMockGearmanJobLib : public mock::libs::gearman::MockGearmanJobLib {
};

namespace mockcurl = mock::libs::curl;
class ConfigurableMockCurlLib : public mock::libs::curl::MockCurlLib {
public:
    ConfigurableMockCurlLib() :
        cleanupCalled(false), formFreed(false), stringsFreed(false),
        initRet(reinterpret_cast<mockcurl::CURLHandle>(1)),
        setOptRet(CURLE_OK), performRet(CURLE_OK),
        getInfoRet(CURLE_OK), formAddRet(CURL_FORMADD_OK) {}

    void configure(CURLcode setOptRet, CURLcode performRet,
                   CURLcode getInfoRet, CURLFORMcode formAddRet) {
        this->setOptRet = setOptRet;
        this->performRet = performRet;
        this->getInfoRet = getInfoRet;
        this->formAddRet = formAddRet;
    }

    void configureInit(mockcurl::CURLHandle initRet) {
        this->initRet = initRet;
    }

    void configureGetInfo(CURLINFO info, void *param) {
        this->infoAction = std::tuple<CURLINFO, void*>(info, param);
    }

    void configureSetOpt(CURLoption opt,
                         std::function<void(void*)> optFunc) {
        this->setOptAction =
            std::tuple<CURLoption, std::function<void(void*)>>(opt, optFunc);
    }

    void reset() {
        this->cleanupCalled = false;
        this->formFreed = false;
        this->stringsFreed = false;
        this->initRet = reinterpret_cast<mockcurl::CURLHandle>(1);
        this->setOptRet = CURLE_OK;
        this->performRet = CURLE_OK;
        this->getInfoRet = CURLE_OK;
        this->formAddRet = CURL_FORMADD_OK;
        this->infoAction = std::tuple<CURLINFO, void*>();
        this->setOptAction = std::tuple<CURLoption, std::function<void(void*)>>();
    }

    bool allCleanupRoutinesCalled() {
        return this->cleanupCalled && this->formFreed && this->stringsFreed;
    }

    mockcurl::CURLHandle init() {
        return this->initRet;
    }

    CURLcode setOpt(mockcurl::CURLHandle handle, CURLoption opt, void *param) {
        if (opt == std::get<0>(this->setOptAction)) {
            auto optFunc = std::get<1>(this->setOptAction);
            if (optFunc) {
                optFunc(param);
            }
        }

        return this->setOptRet;
    }

    CURLFORMcode formAdd(mockcurl::CURLHTTPPost *formPost,
                         mockcurl::CURLHTTPPost *last,
                         const va_list &args) {
        return this->formAddRet;
    }

    CURLcode perform(mockcurl::CURLHandle handle) {
        return this->performRet;
    }

    CURLcode getInfo(mockcurl::CURLHandle handle, CURLINFO info, void *param) {
        if (this->infoActionSet() && info == std::get<0>(this->infoAction)) {
            int type = CURLINFO_TYPEMASK & static_cast<int>(info);
            switch(type) {
            case CURLINFO_LONG:
                *(long*)param = *(static_cast<long*>(std::get<1>(this->infoAction)));
                break;
            case CURLINFO_STRING:
                *(char*)param = *(static_cast<char*>(std::get<1>(this->infoAction)));
                break;
            }
        }

        return this->getInfoRet;
    }

    virtual void cleanup(mockcurl::CURLHandle handle) {
        this->cleanupCalled = true;
    }

    virtual void formFree(mockcurl::CURLHTTPPost formPost) {
        this->formFreed = true;
    }

    virtual void reclaimStringList(mockcurl::CURLStringList list) {
        this->stringsFreed = true;
    }
private:
    bool infoActionSet() {
        return this->infoAction != std::tuple<CURLINFO, void*>();
    }

    bool cleanupCalled;
    bool formFreed;
    bool stringsFreed;

    mockcurl::CURLHandle initRet;
    CURLcode setOptRet;
    CURLcode performRet;
    CURLcode getInfoRet;
    CURLFORMcode formAddRet;

    std::tuple<CURLINFO, void*> infoAction;
    std::tuple<CURLoption, std::function<void(void*)>> setOptAction;
};

class FailedWriter : public Writer {
public:
    size_t write(const char *str, size_t len) {
        throw std::runtime_error("denied");
    }

    virtual std::string str() {
        return "";
    }
};

class GearmanClientTest : public ::testing::Test {
public:
    ConfigurableMockCurlLib mockCurlLib;
    ConfigurableMockGearmanJobLib mockGearmanJobLib;
    ConfigurableMockGearmanWorkerLib mockGearmanWorkerLib;
    ThreadRegistryPtr mockThreadRegistry;
    MockMetricProxyPtr mockMetricProxy;
    MetricProxyPoolWrapperPtr mockMetricProxyPoolWrapper;


    GearmanClientTest() : mockThreadRegistry(new mock::classes::MockThreadRegistry),
                          mockMetricProxy(new mock::classes::MockMetricProxy) {
        mockMetricProxyPoolWrapper = MetricProxyPoolWrapper::wrap("testcase_pool_name", mockMetricProxy);
    }

    void SetUp() {
        mockCurlLib.reset();
        mockGearmanWorkerLib.reset();
        initMockCurlLib(&mockCurlLib);
        initMockGearmanLibs(&mockGearmanJobLib, &mockGearmanWorkerLib);
        mockMetricProxy->reset();
    }
};

const gearman_return_t GEARMAN_FAKE_RET(static_cast<gearman_return_t>(999));

TEST_F(GearmanClientTest, TestGearmanClientThrowsOnBadServerList) {
    gearman_return_t badServerReturn = GEARMAN_INVALID_ARGUMENT;
    mockGearmanWorkerLib.configure(
        GEARMAN_IO_WAIT, GEARMAN_FAKE_RET,
        badServerReturn, GEARMAN_SUCCESS
    );

    StringSet servers;
    servers.insert("server a");
    bool caught = false;
    try {
        std::unique_ptr<GearmanClient>(
            new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, servers, StringSet(), "")
        );
    } catch (const GearmanClientException &e) {
        caught = true;
    }

    ASSERT_TRUE(caught);
}

TEST_F(GearmanClientTest, TestGearmanClientThrowsOnBadJobsList) {
    gearman_return_t badJobsReturn = GEARMAN_INVALID_ARGUMENT;
    mockGearmanWorkerLib.configure(
        GEARMAN_IO_WAIT, GEARMAN_FAKE_RET,
        GEARMAN_SUCCESS, badJobsReturn
    );

    StringSet jobs;
    jobs.insert("job a");
    bool caught = false;
    try {
        std::unique_ptr<GearmanClient>(
            new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), jobs, "")
        );
    } catch (const GearmanClientException &e) {
        caught = true;
    }

    ASSERT_TRUE(caught);
}

TEST_F(GearmanClientTest, TestRunPollsOnWaitOrNoJobs) {
    mockGearmanWorkerLib.configure(
        GEARMAN_IO_WAIT, GEARMAN_FAKE_RET,
        GEARMAN_SUCCESS, GEARMAN_SUCCESS
    );

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    try {
        client->run();
    } catch (const GearmanClientException &e) {}

    ASSERT_EQ(1, mockGearmanWorkerLib.timesWaitCalled);

    mockGearmanWorkerLib.reset();
    mockGearmanWorkerLib.configure(
        GEARMAN_NO_JOBS, GEARMAN_FAKE_RET,
        GEARMAN_SUCCESS, GEARMAN_SUCCESS
    );

    client.reset(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    try {
        client->run();
    } catch (const GearmanClientException &e) {}

    ASSERT_EQ(1, mockGearmanWorkerLib.timesWaitCalled);
}

TEST_F(GearmanClientTest, TestRunThrowsOnTimeoutOrNotConnected) {
    mockGearmanWorkerLib.configure(
        GEARMAN_TIMEOUT, GEARMAN_FAKE_RET,
        GEARMAN_SUCCESS, GEARMAN_SUCCESS
    );

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    bool caught = false;
    try {
        client->run();
    } catch (const GearmanClientException &e) {
        caught = true;
    }

    ASSERT_TRUE(caught);

    caught = false;
    mockGearmanWorkerLib.reset();
    mockGearmanWorkerLib.configure(
        GEARMAN_NOT_CONNECTED, GEARMAN_FAKE_RET,
        GEARMAN_SUCCESS, GEARMAN_SUCCESS
    );

    client.reset(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    try {
        client->run();
    } catch (const GearmanClientException &e) {
        caught = true;
    }

    ASSERT_TRUE(caught);
}

TEST_F(GearmanClientTest, TestRunThrowsOnUnknownWorkReturn) {
    mockGearmanWorkerLib.configure(
        GEARMAN_FAKE_RET, GEARMAN_SUCCESS,
        GEARMAN_SUCCESS, GEARMAN_SUCCESS
    );

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    bool caught = false;
    try {
        client->run();
    } catch (const GearmanClientException &e) {
        caught = true;
    }

    ASSERT_TRUE(caught);
}

TEST_F(GearmanClientTest, TestRunInPollStateGrabsNextJobOnSuccess) {
    mockGearmanWorkerLib.configure(
        GEARMAN_SUCCESS, GEARMAN_SUCCESS,
        GEARMAN_SUCCESS, GEARMAN_SUCCESS
    );

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    try {
        client->run();
    } catch (const std::out_of_range &e) {} // test-specific error
    ASSERT_TRUE(true); // we're fine if control reaches here
}

TEST_F(GearmanClientTest, TestRunInPollStateReturnsOnTimeout) {
    mockGearmanWorkerLib.configure(
        GEARMAN_SUCCESS, GEARMAN_TIMEOUT,
        GEARMAN_SUCCESS, GEARMAN_SUCCESS
    );

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    client->run();
    ASSERT_TRUE(true); // we're fine if control reaches here
}

TEST_F(GearmanClientTest, TestRunInPollStateThrowsOnNoFds) {
    mockGearmanWorkerLib.configure(
        GEARMAN_FAKE_RET, GEARMAN_NO_ACTIVE_FDS,
        GEARMAN_SUCCESS, GEARMAN_SUCCESS
    );

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    bool caught = false;
    try {
        client->run();
    } catch (const GearmanClientException &e) {
        caught = true;
    }

    ASSERT_TRUE(caught);
}

TEST_F(GearmanClientTest, TestRunInPollStateThrowsOnUnkownReturn) {
    mockGearmanWorkerLib.configure(
        GEARMAN_FAKE_RET, GEARMAN_FAKE_RET,
        GEARMAN_SUCCESS, GEARMAN_SUCCESS
    );

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    bool caught = false;
    try {
        client->run();
    } catch (const GearmanClientException &e) {
        caught = true;
    }

    ASSERT_TRUE(caught);
}

TEST_F(GearmanClientTest, TestProcessJobReturnsGearmanErrorOnFailedInit) {
    mockCurlLib.configure(CURLE_OK, CURLE_OK, CURLE_OK, CURL_FORMADD_OK);
    mockCurlLib.configureInit(nullptr);

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    std::string gearmanRet;
    gearman_return_t expectedFailure = client->processJob(nullptr, gearmanRet);
    ASSERT_EQ(GEARMAN_WORK_FAIL, expectedFailure);
    ASSERT_TRUE(mockCurlLib.allCleanupRoutinesCalled());
}

TEST_F(GearmanClientTest, TestProcessJobReturnsGearmanErrorOnFailedSetOpt) {
    mockCurlLib.configure(CURLE_FAILED_INIT, CURLE_OK, CURLE_OK, CURL_FORMADD_OK);

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    std::string gearmanRet;
    gearman_return_t expectedFailure = client->processJob(nullptr, gearmanRet);
    ASSERT_EQ(GEARMAN_WORK_FAIL, expectedFailure);
    ASSERT_TRUE(mockCurlLib.allCleanupRoutinesCalled());
}

TEST_F(GearmanClientTest, TestProcessJobReturnsGearmanErrorOnFailedFormAdd) {
    mockCurlLib.configure(CURLE_OK, CURLE_OK, CURLE_OK, CURL_FORMADD_MEMORY);

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    std::string gearmanRet;
    gearman_return_t expectedFailure = client->processJob(nullptr, gearmanRet);
    ASSERT_EQ(GEARMAN_WORK_FAIL, expectedFailure);
    ASSERT_TRUE(mockCurlLib.allCleanupRoutinesCalled());
}

/* This also tests failure to setsockopt(SO_REUSEADDR).
 * Per https://curl.haxx.se/libcurl/c/CURLOPT_SOCKOPTFUNCTION.html, CURL_SOCKOPT_ERROR
 * from the callback is returned to perform as CURLE_COULDNT_CONNECT
 */
TEST_F(GearmanClientTest, TestProcessJobReturnsGearmanErrorOnFailedPerform) {
    mockCurlLib.configure(CURLE_OK, CURLE_COULDNT_CONNECT, CURLE_OK, CURL_FORMADD_OK);

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    std::string gearmanRet;
    gearman_return_t expectedFailure = client->processJob(nullptr, gearmanRet);
    ASSERT_EQ(GEARMAN_WORK_FAIL, expectedFailure);
    ASSERT_TRUE(mockCurlLib.allCleanupRoutinesCalled());
}

TEST_F(GearmanClientTest, TestProcessJobReturnsGearmanErrorOnNon200Response) {
    mockCurlLib.configure(CURLE_OK, CURLE_OK, CURLE_OK, CURL_FORMADD_OK);

    long serverError(500);
    mockCurlLib.configureGetInfo(CURLINFO_RESPONSE_CODE, &serverError);

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    std::string gearmanRet;
    gearman_return_t expectedFailure = client->processJob(nullptr, gearmanRet);
    ASSERT_EQ(GEARMAN_WORK_FAIL, expectedFailure);
    ASSERT_TRUE(mockCurlLib.allCleanupRoutinesCalled());
}

TEST_F(GearmanClientTest, TestProcessJobReturnsGearmanErrorOnResponseParseFailure) {
    mockCurlLib.configure(CURLE_OK, CURLE_OK, CURLE_OK, CURL_FORMADD_OK);

    long ok(200);
    mockCurlLib.configureGetInfo(CURLINFO_RESPONSE_CODE, &ok);

    char *invalidJson("INVALID JSON");
    auto writeFunc = [invalidJson] (void *userData) {
        curl_write_func(invalidJson, strlen(invalidJson), 1, userData);
    };

    mockCurlLib.configureSetOpt(CURLOPT_WRITEDATA, writeFunc);

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    std::string gearmanRet;
    gearman_return_t expectedFailure = client->processJob(nullptr, gearmanRet);
    ASSERT_EQ(GEARMAN_WORK_FAIL, expectedFailure);
    ASSERT_TRUE(mockCurlLib.allCleanupRoutinesCalled());
}

TEST_F(GearmanClientTest, TestProcessJobReturnsGearmanErrorOnInvalidResponseStructure) {
    mockCurlLib.configure(CURLE_OK, CURLE_OK, CURLE_OK, CURL_FORMADD_OK);

    long ok(200);
    mockCurlLib.configureGetInfo(CURLINFO_RESPONSE_CODE, &ok);

    std::vector<char*> badResponses = {
        "{\"gearman_ret\": 0}", // mising response_string
        "{\"response_string\": \"OK\"}", // mising gearman_ret
        "{\"gearman_ret\": \"NOTANINT\", \"response_string\": \"OK\"}", // invalid gearman_ret
        "{\"gearman_ret\": 0, \"response_string\": {}}", // invalid response_string
    };

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    for (auto badResponse : badResponses) {
        auto writeFunc = [badResponse] (void *userData) {
            curl_write_func(badResponse, strlen(badResponse), 1, userData);
        };

        mockCurlLib.configureSetOpt(CURLOPT_WRITEDATA, writeFunc);
        std::string gearmanRet;
        gearman_return_t expectedFailure = client->processJob(nullptr, gearmanRet);
        ASSERT_EQ(GEARMAN_WORK_FAIL, expectedFailure);
        ASSERT_TRUE(mockCurlLib.allCleanupRoutinesCalled());
    }
}

TEST_F(GearmanClientTest, TestWorkerCallbackProcessesJob) {
    mockCurlLib.configure(CURLE_OK, CURLE_OK, CURLE_OK, CURL_FORMADD_OK);

    long ok(200);
    mockCurlLib.configureGetInfo(CURLINFO_RESPONSE_CODE, &ok);

    std::string testResponseValue("TEST RESPONSE");
    std::stringstream goodResponseStream;
    goodResponseStream << "{\"gearman_ret\": " << GEARMAN_SUCCESS
                       << ", \"response_string\": \"" << testResponseValue
                       << "\"}";

    std::string goodResponse(goodResponseStream.str());
    auto writeFunc = [goodResponse] (void *userData) {
        curl_write_func(
            const_cast<char*>(goodResponse.c_str()), goodResponse.length(),
            1, userData
        );
    };

    mockCurlLib.configureSetOpt(CURLOPT_WRITEDATA, writeFunc);

    std::unique_ptr<GearmanClient> client(
        new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    size_t outSize(0);
    gearman_return_t outRet;
    worker_callback(nullptr, static_cast<void*>(client.get()),
                    &outSize, &outRet);
    ASSERT_EQ(GEARMAN_SUCCESS, outRet);
}

TEST_F(GearmanClientTest, TestWriteCallbackAppendsToStream) {
    std::stringstream stream;
    char *toWrite = "a man, a plan, a canal, panama";
    size_t expectedWriteLen = strlen(toWrite);

    size_t actualWriteLen = curl_write_func(toWrite, expectedWriteLen,
                                            1, static_cast<void*>(&stream));
    ASSERT_EQ(expectedWriteLen, actualWriteLen);
}

TEST_F(GearmanClientTest, TestWriteCallbackReturnsZeroOnException) {
    FailedWriter fail;
    char *toWrite = "a man, a plan, a canal, panama";
    size_t writeLen = strlen(toWrite);

    size_t actualWriteLen = curl_write_func(toWrite, writeLen,
                                            1, static_cast<void*>(&fail));
    ASSERT_EQ(0, actualWriteLen);
}

TEST_F(GearmanClientTest, TestProgressCallbackRespectsShutdownFlag) {
    g_force_shutdown = true;
    int ret = curl_progress_func(nullptr, 0.0, 0.0, 0.0, 0.0);
    ASSERT_EQ(1, ret);
    g_force_shutdown = false;
}

TEST_F(GearmanClientTest, TestProgressCallbackErrorsOnTimeout) {
    // epoch value of 0 guarantees we're in a timed-out state
    time_t startTime(0);
    int ret = curl_progress_func(&startTime, 0.0, 0.0, 0.0, 0.0);
    ASSERT_EQ(1, ret);
}

TEST_F(GearmanClientTest, TestProgressCallbackSuccessOnNormalTimeBound) {
    // playing with time in tests is a losing proposition, but we have a five
    // second window for the timeout value so this is unlikely to be flaky
    time_t startTime(time(nullptr));
    int ret = curl_progress_func(&startTime, 0.0, 0.0, 0.0, 0.0);
    ASSERT_EQ(0, ret);
}

TEST_F(GearmanClientTest, TestSetSockOptOnBadFd) {
    int ret = curl_set_sockopt(nullptr, -1, CURLSOCKTYPE_LAST);
    ASSERT_EQ(CURL_SOCKOPT_ERROR, ret);
    ASSERT_EQ(EBADF, errno);
}


TEST_F(GearmanClientTest, TestProcessJobDelayMetricOnSuccess)
{
    static const double TEST_SLEEP_DURATION_SECONDS = 0.123;

    mockCurlLib.configure(CURLE_OK, CURLE_OK, CURLE_OK, CURL_FORMADD_OK);

    long ok(200);
    mockCurlLib.configureGetInfo(CURLINFO_RESPONSE_CODE, &ok);

    std::string testResponseValue("TEST RESPONSE");
    std::stringstream goodResponseStream;
    goodResponseStream << "{\"gearman_ret\": " << GEARMAN_SUCCESS
                       << ", \"response_string\": \"" << testResponseValue
                       << "\"}";

    std::string goodResponse(goodResponseStream.str());
    auto writeFunc = [goodResponse] (void *userData) {
        usleep(TEST_SLEEP_DURATION_SECONDS * 1000000);
        curl_write_func(
                const_cast<char*>(goodResponse.c_str()), goodResponse.length(),
                1, userData
        );
    };

    mockCurlLib.configureSetOpt(CURLOPT_WRITEDATA, writeFunc);

    std::unique_ptr<GearmanClient> client(
            new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    std::string gearmanReturnValue;
    client->processJob(nullptr, gearmanReturnValue);

    EXPECT_EQ(mockMetricProxy->getJobSuccessesCount("testcase_pool_name", "mocked_function_name"), 1);
    EXPECT_EQ(mockMetricProxy->getJobSuccessesCount("testcase_pool_name", "unknown_function_name"), 0);

    double jobSuccessesDelaySum = mockMetricProxy->getJobSuccessesDelaySum("testcase_pool_name", "mocked_function_name");
    EXPECT_GE(jobSuccessesDelaySum, TEST_SLEEP_DURATION_SECONDS);
}

TEST_F(GearmanClientTest, TestProcessErrorMetricOn500Response) {
    mockCurlLib.configure(CURLE_OK, CURLE_OK, CURLE_OK, CURL_FORMADD_OK);

    long serverError(500);
    mockCurlLib.configureGetInfo(CURLINFO_RESPONSE_CODE, &serverError);

    std::unique_ptr<GearmanClient> client(
            new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    std::string gearmanRet;
    client->processJob(nullptr, gearmanRet);

    // Not successful, no success metric should be reported
    EXPECT_EQ(mockMetricProxy->getJobSuccessesCount("testcase_pool_name", "mocked_function_name"), 0);

    // Expect the 500 http error to have been recorded
    EXPECT_EQ(mockMetricProxy->getJobHttpErrorCount("testcase_pool_name", "mocked_function_name", 500), 1);

    // Expect a generic error to have been recorded ( error count includes
    // all kinds of errors including non-200 http and timeouts captured by other metrics
    EXPECT_EQ(mockMetricProxy->getJobErrorCount("testcase_pool_name", "mocked_function_name"), 1);
}

// Timeouts in our case aren't true "curl" timeouts, but happen as a result of a
// calculation in curl_progress_func comparing the elapsed time to MAX_JOB_RUNNING_TIME
// When the progress function returns non-zero the main curl returns CURLE_ABORTED_BY_CALLBACK
// we use the CURLE_ABORTED_BY_CALLBACK return value to signal updating the timeout metric.
// The timeout is also considered an error and counted in the error metric
TEST_F(GearmanClientTest, TestProcessErrorMetricOnTimeout) {
    mockCurlLib.configure(CURLE_OK, CURLE_ABORTED_BY_CALLBACK, CURLE_OK, CURL_FORMADD_OK);

    std::unique_ptr<GearmanClient> client(
            new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "")
    );

    std::string gearmanRet;
    client->processJob(nullptr, gearmanRet);

    EXPECT_EQ(mockMetricProxy->getJobTimeoutCount("testcase_pool_name", "mocked_function_name"), 1);
    EXPECT_EQ(mockMetricProxy->getJobErrorCount("testcase_pool_name", "mocked_function_name"), 1);
}


// TestThreadMetrics ties together GearmanClient::run and GearmanClient::processJob
// such a manner that they are invoked in the correct order with all the mocks
// enabled.  By doing this we can record the times the 4 thread gauge events:
//   1) a thread starting,
//   2) the transition from idle to working,
//   3) the transition from working to idle,
//   4) a thread ending
// amd ensure that these events occur in the expected order
TEST_F(GearmanClientTest, TestThreadMetrics) {

    mockCurlLib.configure(CURLE_OK, CURLE_OK, CURLE_OK, CURL_FORMADD_OK);

    long ok(200);
    mockCurlLib.configureGetInfo(CURLINFO_RESPONSE_CODE, &ok);

    std::string testResponseValue("TEST RESPONSE");
    std::stringstream goodResponseStream;
    goodResponseStream << "{\"gearman_ret\": " << GEARMAN_SUCCESS
                       << ", \"response_string\": \"" << testResponseValue
                       << "\"}";

    std::string goodResponse(goodResponseStream.str());
    auto writeFunc = [goodResponse] (void *userData) {
        curl_write_func(
                const_cast<char*>(goodResponse.c_str()), goodResponse.length(),
                1, userData
        );
    };

    mockCurlLib.configureSetOpt(CURLOPT_WRITEDATA, writeFunc);

    auto *client = new GearmanClient(mockThreadRegistry, mockMetricProxyPoolWrapper, StringSet(), StringSet(), "");

    auto workFunction = [](GearmanClient *client) {
        std::string gearmanReturnValue;
        client->processJob(nullptr, gearmanReturnValue);
        return GEARMAN_SUCCESS;
    };

    mockGearmanWorkerLib.configure_with_work_function(
            workFunction, client, GEARMAN_SUCCESS,
            GEARMAN_SUCCESS, GEARMAN_SUCCESS
    );

    client->run();

    delete client;

    auto threadStart = mockMetricProxy->popThreadStart("testcase_pool_name");
    auto workStart = mockMetricProxy->popWorkStart("testcase_pool_name", "mocked_function_name");
    auto workEnd = mockMetricProxy->popWorkEnd("testcase_pool_name", "mocked_function_name");
    auto threadEnd = mockMetricProxy->popThreadEnd("testcase_pool_name");

    EXPECT_GT(workStart, threadStart);
    EXPECT_GT(workEnd, workStart);
    EXPECT_GT(threadEnd, workEnd);
}