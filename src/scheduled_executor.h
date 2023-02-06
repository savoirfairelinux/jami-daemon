/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include <thread>
#include <functional>
#include <map>
#include <vector>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <ciso646>

#include "noncopyable.h"

#include "tracepoint.h"
#include "trace-tools.h"

namespace jami {

extern std::atomic<uint64_t> task_cookie;

/**
 * A runnable function
 */
struct Job {
    Job(std::function<void()>&& f, const char* file, uint32_t l)
        : fn(std::move(f))
        , filename(file)
        , linum(l) { }

    std::function<void()> fn;
    const char* filename;
    uint32_t linum;

    inline operator bool() const {
        return static_cast<bool>(fn);
    }

    void reset() {
        fn = {};
    }
};

struct RepeatedJob {
    RepeatedJob(std::function<bool()>&& f, const char* file, uint32_t l)
        : fn(std::move(f))
        , filename(file)
        , linum(l) { }

    std::function<bool()> fn;
    const char* filename;
    uint32_t linum;

    inline operator bool() {
        return static_cast<bool>(fn);
    }

    void reset() {
        fn = {};
    }
};

/**
 * A Job that can be disposed
 */
class Task
{
public:
    Task(std::function<void()>&& fn, const char* filename, uint32_t linum)
        : job_(std::move(fn), filename, linum)
        , cookie_(task_cookie++) { }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
    void run(const char* executor_name)
    {
        if (job_.fn) {
            jami_tracepoint(scheduled_executor_task_begin,
                            executor_name,
                            job_.filename, job_.linum,
                            cookie_);
            job_.fn();
            jami_tracepoint(scheduled_executor_task_end,
                            cookie_);
        }
    }
#pragma GCC pop

    void cancel() { job_.reset(); }
    bool isCancelled() const { return !job_; }

    Job& job() { return job_; }

private:
    Job job_;
    uint64_t cookie_;
};

/**
 * A RepeatedJob that can be disposed
 */
class RepeatedTask
{
public:
    RepeatedTask(std::function<bool()>&& fn, const char* filename,
                 uint32_t linum)
        : job_(std::move(fn), filename, linum)
        , cookie_(task_cookie++) { }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
    bool run(const char* executor_name)
    {
        bool cont;
        std::lock_guard<std::mutex> l(lock_);

        if (not cancel_.load() and job_.fn) {
            jami_tracepoint(scheduled_executor_task_begin,
                            executor_name,
                            job_.filename, job_.linum,
                            cookie_);
            cont = job_.fn();
            jami_tracepoint(scheduled_executor_task_end,
                            cookie_);

        } else {
            cont = false;
        }

        if (not cont) {
            cancel_.store(true);
            job_.reset();
        }

        return static_cast<bool>(job_);
    }
#pragma GCC pop

    void cancel() { cancel_.store(true); }

    void destroy()
    {
        cancel();
        std::lock_guard<std::mutex> l(lock_);
        job_.reset();
    }

    bool isCancelled() const { return cancel_.load(); }

    RepeatedJob& job() { return job_; }

private:
    NON_COPYABLE(RepeatedTask);
    RepeatedJob job_;
    mutable std::mutex lock_;
    std::atomic_bool cancel_ {false};
    uint64_t cookie_;
};

class ScheduledExecutor
{
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = clock::duration;

    ScheduledExecutor(const std::string& name_);
    ~ScheduledExecutor();

    /**
     * Schedule job to be run ASAP
     */
    void run(std::function<void()>&& job,
             const char* filename=CURRENT_FILENAME(),
             uint32_t linum=CURRENT_LINE());

    /**
     * Schedule job to be run at time t
     */
    std::shared_ptr<Task> schedule(std::function<void()>&& job, time_point t,
                                   const char* filename=CURRENT_FILENAME(),
                                   uint32_t linum=CURRENT_LINE());

    /**
     * Schedule job to be run after delay dt
     */
    std::shared_ptr<Task> scheduleIn(std::function<void()>&& job, duration dt,
                                     const char* filename=CURRENT_FILENAME(),
                                     uint32_t linum=CURRENT_LINE());

    /**
     * Schedule job to be run every dt, starting now.
     */
    std::shared_ptr<RepeatedTask> scheduleAtFixedRate(std::function<bool()>&& job,
                                                      duration dt,
                                                      const char* filename=CURRENT_FILENAME(),
                                                      uint32_t linum=CURRENT_LINE());

    /**
     * Stop the scheduler, can't be reversed
     */
    void stop();

private:
    NON_COPYABLE(ScheduledExecutor);

    void loop();
    void schedule(std::shared_ptr<Task>, time_point t);
    void reschedule(std::shared_ptr<RepeatedTask>, time_point t, duration dt);

    std::string name_;
    std::shared_ptr<std::atomic<bool>> running_;
    std::map<time_point, std::vector<Job>> jobs_ {};
    std::mutex jobLock_ {};
    std::condition_variable cv_ {};
    std::thread thread_;
};

} // namespace jami
