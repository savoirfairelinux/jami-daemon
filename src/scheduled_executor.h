/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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

namespace jami {

/**
 * A runnable function
 */
using Job = std::function<void()>;
using RepeatedJob = std::function<bool()>;

/**
 * A Job that can be disposed
 */
class Task
{
public:
    Task(Job&& j)
        : job_(std::move(j))
    {}
    void run()
    {
        if (job_)
            job_();
    }
    void cancel() { job_ = {}; }
    bool isCancelled() const { return !job_; }

private:
    Job job_;
};

/**
 * A RepeatedJob that can be disposed
 */
class RepeatedTask
{
public:
    RepeatedTask(RepeatedJob&& j)
        : job_(std::move(j))
    {}
    bool run()
    {
        std::lock_guard<std::mutex> l(lock_);
        if (cancel_.load() or (job_ and not job_())) {
            cancel_.store(true);
            job_ = {};
        }
        return (bool) job_;
    }
    void cancel() { cancel_.store(true); }
    void destroy()
    {
        cancel();
        std::lock_guard<std::mutex> l(lock_);
        job_ = {};
    }
    bool isCancelled() const { return cancel_.load(); }

private:
    NON_COPYABLE(RepeatedTask);
    mutable std::mutex lock_;
    RepeatedJob job_;
    std::atomic_bool cancel_ {false};
};

class ScheduledExecutor
{
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = clock::duration;

    ScheduledExecutor();
    ~ScheduledExecutor();

    /**
     * Schedule job to be run ASAP
     */
    void run(Job&& job);

    /**
     * Schedule job to be run at time t
     */
    std::shared_ptr<Task> schedule(Job&& job, time_point t);

    /**
     * Schedule job to be run after delay dt
     */
    std::shared_ptr<Task> scheduleIn(Job&& job, duration dt);

    /**
     * Schedule job to be run every dt, starting now.
     * @param job task to run. The repeated job is stopped
     * if job() returns 'false'
     * @param dt the periode between two runs.
     * @return a shared pointer on the scheduled task
     */
    std::shared_ptr<RepeatedTask> scheduleAtFixedRate(RepeatedJob&& job, duration dt);

    /**
     * Stop the scheduler, can't be reversed
     */
    void stop();

private:
    NON_COPYABLE(ScheduledExecutor);

    void loop();
    void schedule(std::shared_ptr<Task>, time_point t);
    void reschedule(std::shared_ptr<RepeatedTask>, time_point t, duration dt);

    std::atomic_bool running_ {true};
    std::map<time_point, std::vector<Job>> jobs_ {};
    std::mutex jobLock_ {};
    std::condition_variable cv_ {};
    std::thread thread_;
};

} // namespace jami
