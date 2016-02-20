#ifndef incl_DRIVESHAFT_MOCK_LIBS_CURL_H_
#define  incl_DRIVESHAFT_MOCK_LIBS_CURL_H_

#include <cstdarg>
#include <curl/curl.h>

namespace mock {
namespace libs {
namespace curl {

typedef CURL* CURLHandle;
typedef struct curl_slist* CURLStringList;
typedef struct curl_httppost* CURLHTTPPost;

class MockCurlLib {
public:
    virtual CURLHandle init() {
        return nullptr;
    }

    virtual CURLStringList append(CURLStringList list, const char *str) {
        return list;
    }

    virtual CURLcode setOpt(CURLHandle handle, CURLoption opt, void *param) {
        return CURLE_OK;
    }

    virtual CURLFORMcode formAdd(CURLHTTPPost *formPost,
                                 CURLHTTPPost *last,
                                 const va_list &args) {
        return CURL_FORMADD_OK;
    }

    virtual CURLcode perform(CURLHandle handle) {
        return CURLE_OK;
    }

    virtual CURLcode getInfo(CURLHandle handle, CURLINFO info, void *param) {
        return CURLE_OK;
    }

    virtual const char* stringifyCode(CURLcode code) {
        return "";
    }

    virtual void cleanup(CURLHandle handle) {}
    virtual void formFree(CURLHTTPPost formPost) {}
    virtual void reclaimStringList(CURLStringList list) {}
};

} // namespace curl
} // namespace libs
} // namespace mock

static mock::libs::curl::MockCurlLib *sMockCurlLib(nullptr);

void initMockCurlLib(mock::libs::curl::MockCurlLib *mockCurlLib) {
    sMockCurlLib = mockCurlLib;
}

CURL* curl_easy_init() {
    return sMockCurlLib->init();
}

struct curl_slist* curl_slist_append(struct curl_slist *list, const char *str) {
    return sMockCurlLib->append(list, str);
}

CURLcode curl_easy_setopt(CURL *handle, CURLoption opt, ...) {
    va_list args;
    va_start(args, opt);
    void *param = va_arg(args, void*);
    va_end(args);
    return sMockCurlLib->setOpt(handle, opt, param);
}

CURLFORMcode curl_formadd(struct curl_httppost **formPost,
                          struct curl_httppost **last,
                          ...) {
    va_list args;
    va_start(args, last);
    CURLFORMcode ret = sMockCurlLib->formAdd(formPost, last, args);
    va_end(args);
    return ret;
}

CURLcode curl_easy_perform(CURL *handle) {
    return sMockCurlLib->perform(handle);
}

CURLcode curl_easy_getinfo(CURL *handle, CURLINFO info, ...) {
    va_list args;
    va_start(args, info);
    void *param = va_arg(args, void*);
    va_end(args);
    return sMockCurlLib->getInfo(handle, info, param);
}

const char* curl_easy_strerror(CURLcode code) {
    return sMockCurlLib->stringifyCode(code);
}

void curl_easy_cleanup(CURL *handle) {
    sMockCurlLib->cleanup(handle);
}

void curl_formfree(struct curl_httppost *formPost) {
    sMockCurlLib->formFree(formPost);
}

void curl_slist_free_all(struct curl_slist *list) {
    sMockCurlLib->reclaimStringList(list);
}

#endif // incl_DRIVESHAFT_MOCK_LIBS_CURL_H_
