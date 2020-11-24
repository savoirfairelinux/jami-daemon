#pragma once

#include <thread>

#include "logger.h"
#if USE_OWN_THREAD
#include "scheduled_executor.h"
#else
#include "manager.h"
#endif

#define CHECK_VALID_THREAD() \
    if (not isValidThread()) \
        JAMI_ERR() << "Invalid thread: " << getCurrentThread() << ". Expected: " << threadId_;

namespace jami {
namespace upnp {

class UpnpThreadUtil
{

public:

protected:

    std::thread::id getCurrentThread() const {
        return std::this_thread::get_id();
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

    std::thread::id threadId_ { 0 };
};

} // namespace upnp
} // namespace jami
