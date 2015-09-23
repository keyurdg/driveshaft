#include <curl/curl.h>
#include <time.h>
#include <string.h>
#include "gearman-client.h"
#include "dist/json/json.h"

namespace Driveshaft {

extern "C" {
static void* worker_callback(gearman_job_st *job, void *context,
                      size_t *result_size,
                      gearman_return_t *ret_ptr) noexcept {
    LOG4CXX_DEBUG(ThreadLogger, "Starting worker callback");

    GearmanClient *cl = (GearmanClient *) context;
    std::string data;
    char* ret;

    *ret_ptr = cl->processJob(job, data);
    *result_size = data.length();
    ret = strndup(data.c_str(), data.length());
    if (ret == nullptr) {
        throw std::bad_alloc(); // Will terminate due to the noexcept
    }

    return ret;
}

static size_t curl_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) noexcept {
    LOG4CXX_DEBUG(ThreadLogger, "Starting curl write callback");
    std::stringstream *write_to = (std::stringstream *) userdata;
    size_t len = size*nmemb;
    try {
        write_to->write(ptr, len);
    } catch (std::exception& e) {
        return 0;
    }

    return len;
}

static const uint64_t MAX_JOB_RUNNING_TIME = 86400; // seconds
static int curl_progress_callback(void *p, double dltotal, double dlnow,
                                  double ultotal, double ulnow) noexcept {
    LOG4CXX_DEBUG(ThreadLogger, "Starting curl progress callback");
    time_t cur_time = time(nullptr);
    time_t *start_time = (time_t *) p;
    if (g_force_shutdown) {
        LOG4CXX_INFO(ThreadLogger, "Global shutdown requested. Aborting in curl_progress_callback");
        return 1;
    }

    if ((*start_time + MAX_JOB_RUNNING_TIME) < cur_time) {
        LOG4CXX_INFO(ThreadLogger, "Job has been running for longer than " << MAX_JOB_RUNNING_TIME << " seconds. Aborting it");
        return 1;
    }

    return 0;
}

}

void gearman_client_deleter(gearman_worker_st *ptr) noexcept {
    gearman_worker_unregister_all(ptr);
    gearman_worker_free(ptr);
}

GearmanClient::GearmanClient(const StringSet& server_list, int64_t timeout, const StringSet& jobs_list, const std::string& uri)
                             : m_http_uri(uri)
                             , m_worker_ptr(gearman_worker_create(nullptr), gearman_client_deleter)
                             , m_json_parser(nullptr)
                             , m_state(State::INIT) {
    LOG4CXX_DEBUG(ThreadLogger, "Starting GearmanClient");
    if (m_worker_ptr.get() == nullptr) {
        throw std::bad_alloc();
    }

    gearman_worker_add_options(m_worker_ptr.get(), GEARMAN_WORKER_NON_BLOCKING);
    gearman_worker_set_timeout(m_worker_ptr.get(), timeout);

    for (auto& server : server_list) {
        if (gearman_worker_add_servers(m_worker_ptr.get(), server.c_str()) != GEARMAN_SUCCESS) {
            throw GearmanClientException("Unable to add server: " + server, true);
        }
    }

    for (auto& job : jobs_list) {
        if (gearman_worker_add_function(m_worker_ptr.get(), job.c_str(), 0, &worker_callback, this) != GEARMAN_SUCCESS) {
            throw GearmanClientException("Unable to add job: " + job, true);
        }
    }

    Json::CharReaderBuilder jsonfactory;
    jsonfactory.strictMode(&jsonfactory.settings_);
    m_json_parser.reset(jsonfactory.newCharReader());

    m_state = State::GRAB_JOB;
}

gearman_return_t GearmanClient::processJob(gearman_job_st *job_ptr, std::string& return_string) noexcept {
    CURL *curl;
    CURLcode curlrc;
    CURLFORMcode formerror;
    struct curl_httppost *formpost = nullptr;
    struct curl_httppost *lastptr = nullptr;
    struct curl_slist *headerlist = nullptr;
    gearman_return_t gearman_ret = GEARMAN_SUCCESS;
    time_t start_ts = time(nullptr);
    std::stringstream raw_resp;
    const char *job_function_name = static_cast<const char *>(gearman_job_function_name(job_ptr));
    const char *job_handle = static_cast<const char *>(gearman_job_handle(job_ptr));
    const char *job_unique = static_cast<const char *>(gearman_job_unique(job_ptr));
    std::string job_workload(static_cast<const char *>(gearman_job_workload(job_ptr)), gearman_job_workload_size(job_ptr));
    char error_buf[CURL_ERROR_SIZE];
    error_buf[0] = 0;

    // The blank Expect header is to solve the issue described here: http://devblog.songkick.com/2012/11/27/a-second-here-a-second-there/
    // The cURL documentation also recommends it in their examples: http://curl.haxx.se/libcurl/c/postit2.html
    static const char expect_buf[] = "Expect:";

    curl = curl_easy_init();
    if (!curl) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to open curl handle.");
        goto error;
    }

    headerlist = curl_slist_append(headerlist, expect_buf);

    /* Options */
    if (curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set curl nosignal");
        goto error;
    }
    if (curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set tcp_nodelay");
        goto error;
    }
    if (curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set tcp_keepalive");
        goto error;
    }
    if (curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set tcp_keepidle");
        goto error;
    }
    if (curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set tcp_keepintvl");
        goto error;
    }
    if (curl_easy_setopt(curl, CURLOPT_URL, m_http_uri.c_str()) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set URL");
        goto error;
    }
    if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION , &curl_write_callback) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set write function");
        goto error;
    }
    if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw_resp) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set write data");
        goto error;
    }
    if (curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set noprogress");
        goto error;
    }
    if (curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, &curl_progress_callback) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set progress function");
        goto error;
    }
    if (curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &start_ts) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set progress data");
        goto error;
    }
    if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set headers");
        goto error;
    }
    if (curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buf) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set errorbuffer");
        goto error;
    }


    /* Post data */
    LOG4CXX_INFO(ThreadLogger, "Starting job: function=" << job_function_name << " handle=" << job_handle << " unique=" << job_unique
                               << " workload=" << job_workload);

    if ((formerror = curl_formadd(&formpost, &lastptr, CURLFORM_PTRNAME, "function_name", CURLFORM_PTRCONTENTS, job_function_name, CURLFORM_END)) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to add function_name to post: " << formerror);
        goto error;
    }
    if ((formerror = curl_formadd(&formpost, &lastptr, CURLFORM_PTRNAME, "job_handle", CURLFORM_PTRCONTENTS, job_handle, CURLFORM_END)) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to add job_handle to post: " << formerror);
        goto error;
    }
    if ((formerror = curl_formadd(&formpost, &lastptr, CURLFORM_PTRNAME, "unique", CURLFORM_PTRCONTENTS, job_unique, CURLFORM_END)) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to add unique to post: " << formerror);
        goto error;
    }
    // The length is pulled separately in case there's a NULL byte in the workload.
    if ((formerror = curl_formadd(&formpost, &lastptr, CURLFORM_PTRNAME, "workload", CURLFORM_PTRCONTENTS, job_workload.c_str(), CURLFORM_CONTENTSLENGTH, job_workload.size(), CURLFORM_END)) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to add workload to post: " << formerror);
        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost) != 0) {
        LOG4CXX_ERROR(ThreadLogger, "Unable to set form POST data");
        goto error;
    }
    /* Do it! */
    curlrc = curl_easy_perform(curl);
    if (curlrc != CURLE_OK) {
        LOG4CXX_ERROR(ThreadLogger, "Failed to perform curl. Error: " << curl_easy_strerror(curlrc) << " Message: " << error_buf);
        goto error;
    } else {
        /* check HTTP response code */
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code != 200) {
            LOG4CXX_ERROR(ThreadLogger, "Invalid HTTP response code. Expecting 200, got " << http_code);
            goto error;
        }
    }

    /* Parse the response */
    {
        Json::Value tree;
        const std::string& raw_resp_str = raw_resp.str();
        const char *resp_begin = raw_resp_str.data();
        const char *resp_end = resp_begin + raw_resp_str.length();
        std::string parse_errors;

        if (!m_json_parser->parse(resp_begin, resp_end, &tree, &parse_errors)) {
            LOG4CXX_ERROR(ThreadLogger, "Failed to parse response. Raw response: " << raw_resp << " Errors: " << parse_errors);
            goto error;
        }

        if (!tree.isMember("gearman_ret") || !tree.isMember("response_string") ||
            !tree["gearman_ret"].isUInt() || !tree["response_string"].isString()) {
            LOG4CXX_ERROR(ThreadLogger, "Malformed response from worker. Invalid elements (gearman_ret, response_string): " << raw_resp.str());
            goto error;
        }

        // the Json interface needs a default, so here goes...
        gearman_ret = (gearman_return_t)(tree["gearman_ret"].asInt());
        return_string.append(tree["response_string"].asString());

        LOG4CXX_INFO(ThreadLogger, "Finished job: function=" << job_function_name << " handle=" << job_handle << " unique=" << job_unique
                                   << " workload=" << job_workload
                                   << " return_code=" << gearman_ret << " response_string=" << return_string);

        goto cleanup;
    }

error:
    gearman_ret = GEARMAN_WORK_FAIL;
cleanup:
    curl_easy_cleanup(curl);
    curl_formfree(formpost);
    curl_slist_free_all(headerlist);
    return gearman_ret;
}

void GearmanClient::run() {
    while (true) {
        switch (m_state) {
        case State::INIT:
            throw std::runtime_error("Invalid state found in run");

        case State::GRAB_JOB:
        {
            auto ret = gearman_worker_work(m_worker_ptr.get());
            switch(ret) {
            case GEARMAN_IO_WAIT:
            case GEARMAN_NO_JOBS:
                m_state = State::POLL;
                continue; // We do not want to exit here. Call wait() to get a response from gearmand

            case GEARMAN_WORK_ERROR:
            case GEARMAN_WORK_DATA:
            case GEARMAN_WORK_WARNING:
            case GEARMAN_WORK_STATUS:
            case GEARMAN_WORK_EXCEPTION:
            case GEARMAN_WORK_FAIL:
                LOG4CXX_INFO(ThreadLogger, "Job failed with error " << ret);
                /* fall through */
            case GEARMAN_SUCCESS:
                return; // The caller should decide whether to get more jobs or do other things

            case GEARMAN_TIMEOUT:
            case GEARMAN_NOT_CONNECTED:
            {
                const char *gearman_error = gearman_worker_error(m_worker_ptr.get());
                throw GearmanClientException(std::string("Timeout/disconnected from work(). Error: ") +
                                                std::string(gearman_error ?: "No details"),
                                             false);
            }
            default:
            {
                const char *gearman_error = gearman_worker_error(m_worker_ptr.get());
                throw GearmanClientException(std::string("Unexpected return code from work(): ") +
                                                std::to_string(ret) + ". Details: " +
                                                std::string(gearman_error ?: "No details"),
                                             false);
            }
            }

            break; // Unnecessary
        }

        case State::POLL:
        {
            auto ret = gearman_worker_wait(m_worker_ptr.get());
            switch (ret) {
            case GEARMAN_SUCCESS:
                m_state = State::GRAB_JOB;
                continue; // We should work() to process the received packet

            case GEARMAN_TIMEOUT:
                return; // The caller should decide whether to wait() or do other things

            case GEARMAN_NO_ACTIVE_FDS:
                throw GearmanClientException(std::string("Looks like all gearmand went away"), false);

            default:
                throw GearmanClientException(std::string("Unexpected return code from wait()") + std::to_string(ret), false);
            }
            break; // Unnecessary
        }
        }
    }
}

} // namespace Driveshaft
