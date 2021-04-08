#pragma once

#include <thread>

#include "logger.h"
#include "manager.h"

// This macro is used to validate that a code is executed from the expected
// thread. It's useful to detect unexpected race on data members.
#ifndef CHECK_VALID_THREAD
#define CHECK_VALID_THREAD() \
    if (not isValidThread()) \
        JAMI_ERR() << "The calling thread " << getCurrentThread() \
                   << " is not the expected thread: " << threadId_;
#endif

namespace jami {
namespace upnp {

class UpnpThreadUtil
{
protected:
    std::thread::id getCurrentThread() const { return std::this_thread::get_id(); }

    bool isValidThread() const { return threadId_ == getCurrentThread(); }

    // Upnp context execution queue (same as manager's scheduler)
    // Helpers to run tasks on upnp context queue.
    static ScheduledExecutor* getScheduler() { return &Manager::instance().scheduler(); }
    template<typename Callback>
    static void runOnUpnpContextQueue(Callback&& cb)
    {
        getScheduler()->run([cb = std::forward<Callback>(cb)]() mutable { cb(); });
    }

    std::thread::id threadId_;
};

} // namespace upnp
} // namespace jami
