#pragma once

#include <thread>

#include "logger.h"
#include "manager.h"

// This macro is used to validate that a code is executed from the expected
// thread. It's useful to detect unexpected race on data members.
#define CHECK_VALID_THREAD() \
    if (not isValidThread()) \
        JAMI_ERR() << "The calling thread " << getCurrentThread() << "is not the expected thread" \
                   << threadId_;

namespace jami {
namespace upnp {

class UpnpThreadUtil
{
public:
protected:
    std::thread::id getCurrentThread() const { return std::this_thread::get_id(); }

    bool isValidThread() const { return threadId_ == getCurrentThread(); }

    // Upnp scheduler (same as manager's thread)
    static ScheduledExecutor* getScheduler() { return &Manager::instance().scheduler(); }

    // Helper to run tasks on upnp thread.
    template<typename Callback>
    static void runOnUpnpThread(Callback&& cb)
    {
        getScheduler()->run([cb = std::forward<Callback>(cb)]() mutable { cb(); });
    }

    std::thread::id threadId_ {0};
};

} // namespace upnp
} // namespace jami
