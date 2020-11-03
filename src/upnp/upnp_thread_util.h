#pragma once

#include <sys/syscall.h>

#include "logger.h"
#if USE_OWN_THREAD
#include "scheduled_executor.h"
#else
#include "manager.h"
#endif

#ifdef __linux__

// This macro is used to validate that a code is executed from the expected
// thread. It's useful to detect unexpected race on data members.
#define CHECK_VALID_THREAD() { \
        if (not isValidThread()) \
            JAMI_ERR("The calling thread %lu is not the expected thread %lu", \
                getCurrentThread(), threadId_); \
    }

namespace jami {
namespace upnp {

class UpnpThreadUtil
{

public:

protected:

    long getCurrentThread() const {
        return syscall(__NR_gettid) & 0xffff;
    }

    bool isValidThread() const {
        return threadId_ == getCurrentThread();
    }

    // Upnp scheduler
#if USE_OWN_THREAD
    static std::shared_ptr<ScheduledExecutor> getScheduler() {
        static auto scheduler = std::make_shared<ScheduledExecutor>();
        return scheduler;
    }
#else
    static ScheduledExecutor* getScheduler() {
        return &Manager::instance().scheduler();
    }
#endif

    // Helper to run tasks on upnp thread.
    template<typename Callback>
    static void runOnUpnpThread(Callback&& cb) {
        getScheduler()->run([cb = std::forward<Callback>(cb)]() mutable { cb(); });
    }

    long threadId_ { 0 };
};

#else
    // TODO. Implement.
    #error "Unsupported platform"
#endif // __linux__

} // namespace upnp
} // namespace jami
