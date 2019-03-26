/*
 *  Copyright (C) 2013-2019 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Eloi Bail <Eloi.Bail@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#pragma once

#include <atomic>
#include <thread>
#include <functional>
#include <stdexcept>
#include <condition_variable>
#include <mutex>

namespace ring {

struct ThreadLoopException : public std::runtime_error {
    ThreadLoopException() : std::runtime_error("ThreadLoopException") {}
};

class ThreadLoop {
public:
    enum ThreadState {READY, RUNNING, STOPPING};

    ThreadLoop(const std::function<bool()>& setup,
               const std::function<void()>& process,
               const std::function<void()>& cleanup);

    ThreadLoop(ThreadLoop&&);

    virtual ~ThreadLoop();

    void start();
    void exit();
    virtual void stop();
    void join();
    void waitForCompletion(); // thread will stop itself

    bool isRunning() const noexcept;
    bool isStopping() const noexcept;
    std::thread::id get_id() const noexcept;

private:
    // These must be provided by users of ThreadLoop
    std::function<bool()> setup_;
    std::function<void()> process_;
    std::function<void()> cleanup_;

    void mainloop(std::thread::id& tid,
                  const std::function<bool()> setup,
                  const std::function<void()> process,
                  const std::function<void()> cleanup);

    std::atomic<ThreadState> state_ {READY};
    std::thread::id threadId_;
    std::thread thread_;
};

class InterruptedThreadLoop : public ThreadLoop {
public:

    InterruptedThreadLoop(const std::function<bool()>& setup,
                          const std::function<void()>& process,
                          const std::function<void()>& cleanup)
        : ThreadLoop::ThreadLoop(setup, process, cleanup) {}

    void stop() override;

    void interrupt() noexcept
    {
        cv_.notify_one();
    }

    template <typename Rep, typename Period>
    void
    wait_for(const std::chrono::duration<Rep, Period>& rel_time)
    {
        if (std::this_thread::get_id() != get_id())
            throw std::runtime_error("can not call wait_for outside thread context");

        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait_for(lk, rel_time, [this](){return isStopping();});
    }

    template <typename Rep, typename Period, typename Pred>
    bool
    wait_for(const std::chrono::duration<Rep, Period>& rel_time, Pred&& pred)
    {
        if (std::this_thread::get_id() != get_id())
            throw std::runtime_error("can not call wait_for outside thread context");

        std::unique_lock<std::mutex> lk(mutex_);
        return cv_.wait_for(lk, rel_time, [this, pred]{ return isStopping() || pred(); });
    }

    template <typename Pred>
    void
    wait(Pred&& pred)
    {
        if (std::this_thread::get_id() != get_id())
            throw std::runtime_error("Can not call wait outside thread context");

        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait(lk, [this, p = std::forward<Pred>(pred)]{ return isStopping() || p(); });
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace ring
