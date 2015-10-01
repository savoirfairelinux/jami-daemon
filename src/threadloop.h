/*
 *  Copyright (C) 2013-2015 Savoir-faire Linux Inc.
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

// FIXME: this is ugly
// If condition A is false, print the error message in M and exit thread
#define EXIT_IF_FAIL(A, M, ...) if (!(A)) { \
        RING_ERR(M, ##__VA_ARGS__); loop_.exit(); }

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

    ~ThreadLoop();

    void start();
    void exit();
    void stop();
    void join();

    bool isRunning() const noexcept;

private:
    // These must be provided by users of ThreadLoop
    std::function<bool()> setup_;
    std::function<void()> process_;
    std::function<void()> cleanup_;

    void mainloop(const std::function<bool()> setup,
                  const std::function<void()> process,
                  const std::function<void()> cleanup);

    std::atomic<ThreadState> state_ {READY};
    std::thread thread_;
};

class InterruptedThreadLoop : ThreadLoop {
public:
    InterruptedThreadLoop(const std::function<bool()>& setup,
                          const std::function<void()>& process,
                          const std::function<void()>& cleanup);

    ~InterruptedThreadLoop();

    void start();
    void exit();
    void join();
    bool isRunning() const noexcept;
    void stop();

    //void wait_for(const std::chrono::seconds rel_time);
    template <typename Rep, typename Period>
    void
    wait_for(const std::chrono::duration<Rep, Period>& rel_time)
    {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait_for(lk, rel_time, [&](){return interrupted_;});
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool interrupted_ {false};
};

} // namespace ring
