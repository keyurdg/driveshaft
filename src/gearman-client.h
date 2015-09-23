#ifndef incl_DRIVESHAFT_GEARMAN_CLIENT_H_
#define incl_DRIVESHAFT_GEARMAN_CLIENT_H_

#include <exception>
#include <memory>
#include <libgearman-1.0/gearman.h>
#include "common-defs.h"
#include "dist/json/json.h"

namespace Driveshaft {

void gearman_client_deleter(gearman_worker_st *ptr) noexcept;

class GearmanClient {

public:
    GearmanClient(const StringSet& server_list, int64_t timeout, const StringSet& jobs_list, const std::string& http_uri);
    ~GearmanClient() = default;

    void run();
    gearman_return_t processJob(gearman_job_st *job_ptr, std::string& data) noexcept;

private:
    GearmanClient() = delete;
    GearmanClient(const GearmanClient&) = delete;
    GearmanClient(GearmanClient&&) = delete;
    GearmanClient& operator=(const GearmanClient&) = delete;
    GearmanClient& operator=(const GearmanClient&&) = delete;

    const std::string& m_http_uri;
    std::unique_ptr<gearman_worker_st, decltype(&gearman_client_deleter)> m_worker_ptr;
    std::unique_ptr<Json::CharReader> m_json_parser;
    enum class State {
        INIT,
        GRAB_JOB,
        POLL
    } m_state;
};

class GearmanClientException : public std::exception {

public:
    GearmanClientException(const std::string& msg, bool retriable) noexcept
    : std::exception()
    , m_msg(msg)
    , m_retriable(retriable) {}
    virtual ~GearmanClientException() noexcept {}

    const char* what() const noexcept override {
        return m_msg.c_str();
    }

    bool retriable() const noexcept {
        return m_retriable;
    }

protected:
    std::string m_msg;
    bool m_retriable;
};

}

#endif // incl_DRIVESHAFT_GEARMAN_CLIENT_H_
