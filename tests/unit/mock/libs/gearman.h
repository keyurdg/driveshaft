#ifndef incl_DRIVESHAFT_MOCK_GEARMAN_WORKER_LIB_H_
#define incl_DRIVESHAFT_MOCK_GEARMAN_WORKER_LIB_H_

#include <memory>
#include <libgearman-1.0/gearman.h>

namespace mock {
namespace libs {
namespace gearman {

class MockGearmanWorkerLib {
public:
    virtual gearman_worker_st* create(gearman_worker_st *worker) {
        return nullptr;
    }

    virtual void free(gearman_worker_st *worker) {
    }

    virtual gearman_return_t unregisterAll(gearman_worker_st *worker) {
        return GEARMAN_SUCCESS;
    }

    virtual gearman_return_t addServers(gearman_worker_st *worker, const char *servers) {
        return GEARMAN_SUCCESS;
    }

    virtual gearman_return_t addFunction(gearman_worker_st *worker,
                                 const char *functionName, uint32_t timeout,
                                 gearman_worker_fn *function, void *context) {
        return GEARMAN_SUCCESS;
    }

    virtual void addOptions(gearman_worker_st *worker,
                    gearman_worker_options_t options) {
    }

    virtual void setTimeout(gearman_worker_st *worker, int timeout) {
    }

    virtual gearman_return_t work(gearman_worker_st *worker) {
        return GEARMAN_SUCCESS;
    }

    virtual gearman_return_t wait(gearman_worker_st *worker) {
        return GEARMAN_SUCCESS;
    }

    virtual const char* lastError(const gearman_worker_st *worker) {
        return "";
    }
};

class MockGearmanJobLib {
public:
    virtual const char* functionName(const gearman_job_st *job) {
        return "mocked_function_name";
    }

    virtual const char* handle(const gearman_job_st *job) {
        return "";
    }

    virtual const char* unique(const gearman_job_st *job) {
        return "";
    }

    virtual size_t workloadSize(const gearman_job_st *job) {
        return 0;
    }

    virtual const void* workload(const gearman_job_st *job) {
        return nullptr;
    }
};

} // namespace gearman
} // namespace libs
} // namespace mock

static mock::libs::gearman::MockGearmanJobLib *sMockJobLib(nullptr);
static mock::libs::gearman::MockGearmanWorkerLib *sMockWorkerLib(nullptr);


void initMockGearmanLibs(mock::libs::gearman::MockGearmanJobLib *jobLib,
                         mock::libs::gearman::MockGearmanWorkerLib *workerLib) {
    sMockJobLib = jobLib;
    sMockWorkerLib = workerLib;
}

// mock out gearman lib
gearman_worker_st* gearman_worker_create(gearman_worker_st *worker) {
    return sMockWorkerLib->create(worker);
}

void gearman_worker_free(gearman_worker_st *worker) {
    sMockWorkerLib->free(worker);
}

gearman_return_t gearman_worker_unregister_all(gearman_worker_st *worker) {
    return sMockWorkerLib->unregisterAll(worker);
}

void gearman_worker_add_options(gearman_worker_st *worker, gearman_worker_options_t options) {
    sMockWorkerLib->addOptions(worker, options);
}
gearman_return_t gearman_worker_add_servers(gearman_worker_st *worker, const char *servers) {
    return sMockWorkerLib->addServers(worker, servers);
}

void gearman_worker_set_timeout(gearman_worker_st *worker, int timeout) {
    sMockWorkerLib->setTimeout(worker, timeout);
}
gearman_return_t gearman_worker_add_function(gearman_worker_st *worker, const char *functionName, uint32_t timeout, gearman_worker_fn *function, void *context) {
    return sMockWorkerLib->addFunction(worker, functionName, timeout, function, context);
}

gearman_return_t gearman_worker_work(gearman_worker_st *worker) {
    return sMockWorkerLib->work(worker);
}

gearman_return_t gearman_worker_wait(gearman_worker_st *worker) {
    return sMockWorkerLib->wait(worker);
}

const char* gearman_worker_error(const gearman_worker_st *worker) {
    return sMockWorkerLib->lastError(worker);
}

const char* gearman_job_function_name(const gearman_job_st *job) {
    return sMockJobLib->functionName(job);
}

const char* gearman_job_handle(const gearman_job_st *job) {
    return sMockJobLib->handle(job);
}

const char* gearman_job_unique(const gearman_job_st *job) {
    return sMockJobLib->unique(job);
}

size_t gearman_job_workload_size(const gearman_job_st *job) {
    return sMockJobLib->workloadSize(job);
}

const void* gearman_job_workload(const gearman_job_st *job) {
    return sMockJobLib->workload(job);
}

#endif // incl_DRIVESHAFT_MOCK_GEARMAN_WORKER_LIB_H_
