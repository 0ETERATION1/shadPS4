#include "core/hle/libraries/libkernel/thread_management.h"

#include <core/hle/libraries/libs.h>

#include "common/debug.h"
#include "common/log.h"
#include "core/hle/error_codes.h"

namespace Core::Libraries::LibKernel {

thread_local PthreadInternal g_pthread_self{};
PThreadCxt* g_pthread_cxt = nullptr;

constexpr bool log_pthread_file = true;  // disable it to disable logging

void init_pthreads() {
    g_pthread_cxt = new PThreadCxt{};
    ScePthreadMutexattr default_mutexattr = nullptr;
    scePthreadMutexattrInit(&default_mutexattr);
    g_pthread_cxt->setDefaultMutexattr(default_mutexattr);
}

void Pthread_Init_Self_MainThread() {
    scePthreadAttrInit(&g_pthread_self.attr);
    g_pthread_self.pth = pthread_self();
    g_pthread_self.name = "Main_Thread";
}

int PS4_SYSV_ABI scePthreadAttrInit(ScePthreadAttr* attr) {
    *attr = new PthreadAttrInternal{};

    int result = pthread_attr_init(&(*attr)->pth_attr);

    (*attr)->affinity = 0x7f;
    (*attr)->guard_size = 0x1000;

    SceKernelSchedParam param{};
    param.sched_priority = 700;

    result = (result == 0 ? scePthreadAttrSetinheritsched(attr, 4) : result);
    result = (result == 0 ? scePthreadAttrSetschedparam(attr, &param) : result);
    result = (result == 0 ? scePthreadAttrSetschedpolicy(attr, SCHED_OTHER) : result);
    result = (result == 0 ? scePthreadAttrSetdetachstate(attr, PTHREAD_CREATE_JOINABLE) : result);

    switch (result) {
        case 0: return SCE_OK;
        case ENOMEM: return SCE_KERNEL_ERROR_ENOMEM;
        default: return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadAttrSetdetachstate(ScePthreadAttr* attr, int detachstate) {
    if (attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int pstate = PTHREAD_CREATE_JOINABLE;
    switch (detachstate) {
        case 0: pstate = PTHREAD_CREATE_JOINABLE; break;
        case 1: pstate = PTHREAD_CREATE_DETACHED; break;
        default: BREAKPOINT();  // unknown state
    }

    int result = pthread_attr_setdetachstate(&(*attr)->pth_attr, pstate);

    (*attr)->detached = (pstate == PTHREAD_CREATE_DETACHED);

    if (result == 0) {
        return SCE_OK;
    }
    return SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadAttrSetinheritsched(ScePthreadAttr* attr, int inheritSched) {
    if (attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int pinherit_sched = PTHREAD_INHERIT_SCHED;
    switch (inheritSched) {
        case 0: pinherit_sched = PTHREAD_EXPLICIT_SCHED; break;
        case 4: pinherit_sched = PTHREAD_INHERIT_SCHED; break;
        default: BREAKPOINT();  // unknown inheritSched
    }

    int result = pthread_attr_setinheritsched(&(*attr)->pth_attr, pinherit_sched);

    if (result == 0) {
        return SCE_OK;
    }
    return SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadAttrSetschedparam(ScePthreadAttr* attr, const SceKernelSchedParam* param) {
    if (param == nullptr || attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    SceKernelSchedParam pparam{};
    if (param->sched_priority <= 478) {
        pparam.sched_priority = +2;
    } else if (param->sched_priority >= 733) {
        pparam.sched_priority = -2;
    } else {
        pparam.sched_priority = 0;
    }

    int result = pthread_attr_setschedparam(&(*attr)->pth_attr, &pparam);

    if (result == 0) {
        return SCE_OK;
    }
    return SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadAttrSetschedpolicy(ScePthreadAttr* attr, int policy) {
    if (attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    if (policy != SCHED_OTHER) {
        BREAKPOINT();  // invest if policy is other and if winpthreadlibrary support it
    }

    (*attr)->policy = policy;

    int result = pthread_attr_setschedpolicy(&(*attr)->pth_attr, policy);

    if (result == 0) {
        return SCE_OK;
    }
    return SCE_KERNEL_ERROR_EINVAL;
}

/****
 * Mutex calls
 */
int PS4_SYSV_ABI scePthreadMutexInit(ScePthreadMutex* mutex, const ScePthreadMutexattr* attr, const char* name) {
    PRINT_FUNCTION_NAME();
    if (mutex == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    if (attr == nullptr) {
        attr = g_pthread_cxt->getDefaultMutexattr();
    }

    *mutex = new PthreadMutexInternal{};
    if (name != nullptr) {
        (*mutex)->name = name;
    }

    int result = pthread_mutex_init(&(*mutex)->pth_mutex, &(*attr)->pth_mutex_attr);

    if (name != nullptr) {
        LOG_INFO_IF(log_pthread_file, "mutex_init name={},result={}\n");
    }

    switch (result) {
        case 0: return SCE_OK;
        case EAGAIN: return SCE_KERNEL_ERROR_EAGAIN;
        case EINVAL: return SCE_KERNEL_ERROR_EINVAL;
        case ENOMEM: return SCE_KERNEL_ERROR_ENOMEM;
        default: return SCE_KERNEL_ERROR_EINVAL;
    }
}
int PS4_SYSV_ABI scePthreadMutexattrInit(ScePthreadMutexattr* attr) {
    *attr = new PthreadMutexattrInternal{};

    int result = pthread_mutexattr_init(&(*attr)->pth_mutex_attr);

    result = (result == 0 ? scePthreadMutexattrSettype(attr, 1) : result);
    result = (result == 0 ? scePthreadMutexattrSetprotocol(attr, 0) : result);

    switch (result) {
        case 0: return SCE_OK;
        case ENOMEM: return SCE_KERNEL_ERROR_ENOMEM;
        default: return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadMutexattrSettype(ScePthreadMutexattr* attr, int type) {
    int ptype = PTHREAD_MUTEX_DEFAULT;
    switch (type) {
        case 1: ptype = PTHREAD_MUTEX_ERRORCHECK; break;
        case 2: ptype = PTHREAD_MUTEX_RECURSIVE; break;
        case 3:
        case 4: ptype = PTHREAD_MUTEX_NORMAL; break;
        default: LOG_TRACE_IF(log_pthread_file, "scePthreadMutexattrSettype invalid type: {}\n", type); std::exit(0);
    }

    int result = pthread_mutexattr_settype(&(*attr)->pth_mutex_attr, ptype);

    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadMutexattrSetprotocol(ScePthreadMutexattr* attr, int protocol) {
    int pprotocol = PTHREAD_PRIO_NONE;
    switch (protocol) {
        case 0: pprotocol = PTHREAD_PRIO_NONE; break;
        case 1: pprotocol = PTHREAD_PRIO_INHERIT; break;
        case 2: pprotocol = PTHREAD_PRIO_PROTECT; break;
        default: LOG_TRACE_IF(log_pthread_file, "scePthreadMutexattrSetprotocol invalid protocol: {}\n", protocol); std::exit(0);
    }

    int result = 0;  // pthread_mutexattr_setprotocol(&(*attr)->p, pprotocol); //it appears that pprotocol has issues in winpthreads
    (*attr)->pprotocol = pprotocol;

    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI posix_pthread_mutex_init(ScePthreadMutex* mutex, const ScePthreadMutexattr* attr) {
    LOG_INFO_IF(log_pthread_file, "posix pthread_mutex_init redirect to scePthreadMutexInit\n");
    int result = scePthreadMutexInit(mutex, attr, nullptr);
    if (result < 0) {
        BREAKPOINT();  // posix calls different only for their return values
    }
    return result;
}

void pthreadSymbolsRegister(Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("cmo1RIYva9o", "libkernel", 1, "libkernel", 1, 1, scePthreadMutexInit);
    LIB_FUNCTION("F8bUHwAG284", "libkernel", 1, "libkernel", 1, 1, scePthreadMutexattrInit);
    LIB_FUNCTION("iMp8QpE+XO4", "libkernel", 1, "libkernel", 1, 1, scePthreadMutexattrSettype);
    LIB_FUNCTION("1FGvU0i9saQ", "libkernel", 1, "libkernel", 1, 1, scePthreadMutexattrSetprotocol);
    // posix calls
    LIB_FUNCTION("ttHNfU+qDBU", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_mutex_init);
}

}  // namespace Core::Libraries::LibKernel
