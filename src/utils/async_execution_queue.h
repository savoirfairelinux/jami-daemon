#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __linux__
#include <unistd.h>
#include <sys/syscall.h>
#endif // __linux__

#include "scheduled_executor.h"
#include "logger.h"

#include <thread>
#include <assert.h>

namespace jami {

// This macro validates that a code is executed on the expected
// thread.
#ifndef CHECK_VALID_EXEC_THREAD
#define CHECK_VALID_EXEC_THREAD() \
    if (not isValidThread()) \
        JAMI_ERR() << "[" << __FUNCTION__ << "] " \
                   << " called on wrong thread: " << getCurrentThread() \
                   << " expected: " << getThreadId();
#endif

class AsyncExecutionQueue
{
public:
    std::shared_ptr<ScheduledExecutor> getScheduler() const { return scheduler_; }

    template<typename Callback>
    void runOnExecQueue(Callback&& cb)
    {
        assert(scheduler_);
        scheduler_->run([cb = std::forward<Callback>(cb)]() mutable { cb(); });
    }

    template<typename T, typename Callback>
    void runOnExecQueueW(std::weak_ptr<T> wPtr, Callback&& cb)
    {
        assert(scheduler_);
        scheduler_->run([w = wPtr, cb = std::forward<Callback>(cb)]() mutable {
            std::shared_ptr<T> sharedPtr = w.lock();
            if (not sharedPtr)
                return;
            cb();
        });
    }

    void initialize(std::shared_ptr<ScheduledExecutor> scheduler)
    {
        assert(scheduler != nullptr);
        assert(scheduler_ == nullptr);
        assert(not initialized_);
        scheduler_ = scheduler;

        runOnExecQueue([this, scheduler] {
            std::lock_guard<std::mutex> lock(mutex_);
            threadId_ = getCurrentThread();
            initialized_ = true;
            initCv_.notify_one();
        });

        std::unique_lock<std::mutex> lock(mutex_);
        initCv_.wait(lock, [this] { return initialized_; });
    }

    bool initialized() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return initialized_;
    }

#ifdef __linux__
    long getThreadId() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return threadId_;
    }
#else
    std::thread::id getThreadId() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return threadId_;
    }
#endif // __linux__

#ifdef __linux__
    long getCurrentThread() const { return syscall(__NR_gettid) & 0xffff; }
#else
    std::thread::id getCurrentThread() const { return std::this_thread::get_id(); }
#endif // __linux__

    bool isValidThread() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (not initialized_) {
            JAMI_ERR("Async Queue not initialized yet!");
        }
        return threadId_ == getCurrentThread();
    }

private:
    // The user of this class is responsible for the life-cycle of the scheduler.
    std::shared_ptr<ScheduledExecutor> scheduler_;
    bool initialized_ {false};
    std::condition_variable initCv_;
    mutable std::mutex mutex_;

#ifdef __linux__
    std::atomic<long> threadId_ {0};
#else
    std::thread::id threadId_;
#endif
};

} // namespace jami
