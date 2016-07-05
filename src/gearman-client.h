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

#ifndef incl_DRIVESHAFT_GEARMAN_CLIENT_H_
#define incl_DRIVESHAFT_GEARMAN_CLIENT_H_

#include <exception>
#include <memory>
#include <libgearman-1.0/gearman.h>
#include "common-defs.h"
#include "thread-registry.h"
#include "dist/json/json.h"
#include <curl/curl.h>

namespace Driveshaft {

void* worker_callback(gearman_job_st *job, void *context,
                      size_t *result_size,
                      gearman_return_t *ret_ptr) noexcept;
size_t curl_write_func(char *ptr, size_t size, size_t nmemb, void *userdata) noexcept;
int curl_progress_func(void *p, double dltotal, double dlnow,
                       double ultotal, double ulnow) noexcept;
int curl_set_sockopt(void *unused1, curl_socket_t curlfd, curlsocktype unused2) noexcept;

void gearman_client_deleter(gearman_worker_st *ptr) noexcept;

class Writer {
public:
    // throws on failure
    virtual size_t write(const char *str, size_t len) = 0;
    virtual std::string str() = 0;
};

class GearmanClient {
public:
    GearmanClient(ThreadRegistryPtr registry, const StringSet& server_list, const StringSet& jobs_list, const std::string& http_uri);
    ~GearmanClient() = default;

    void run();
    gearman_return_t processJob(gearman_job_st *job_ptr, std::string& data) noexcept;

private:
    GearmanClient() = delete;
    GearmanClient(const GearmanClient&) = delete;
    GearmanClient(GearmanClient&&) = delete;
    GearmanClient& operator=(const GearmanClient&) = delete;
    GearmanClient& operator=(const GearmanClient&&) = delete;

    ThreadRegistryPtr m_registry;
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
