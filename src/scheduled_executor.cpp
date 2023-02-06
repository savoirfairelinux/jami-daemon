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
#include "scheduled_executor.h"
#include "logger.h"

namespace jami {

std::atomic<uint64_t> task_cookie = {0};

ScheduledExecutor::ScheduledExecutor(const std::string& name)
    : name_(name)
    , running_(std::make_shared<std::atomic<bool>>(true))
    , thread_([this, is_running = running_] {
        // The thread needs its own reference of `running_` in case the
        // scheduler is destroyed within the thread because of a job

        while (*is_running)
            loop();
    })
{}

ScheduledExecutor::~ScheduledExecutor()
{
    stop();

    if (not thread_.joinable()) {
        return;
    }

    // Avoid deadlock
    if (std::this_thread::get_id() == thread_.get_id()) {
        thread_.detach();
    } else {
        thread_.join();
    }
}

void
ScheduledExecutor::stop()
{
    std::lock_guard<std::mutex> lock(jobLock_);
    *running_ = false;
    jobs_.clear();
    cv_.notify_all();
}

void
ScheduledExecutor::run(std::function<void()>&& job,
                       const char* filename, uint32_t linum)
{
    std::lock_guard<std::mutex> lock(jobLock_);
    auto now = clock::now();
    jobs_[now].emplace_back(std::move(job), filename, linum);
    cv_.notify_all();
}

std::shared_ptr<Task>
ScheduledExecutor::schedule(std::function<void()>&& job, time_point t,
                            const char* filename, uint32_t linum)
{
    auto ret = std::make_shared<Task>(std::move(job), filename, linum);
    schedule(ret, t);
    return ret;
}

std::shared_ptr<Task>
ScheduledExecutor::scheduleIn(std::function<void()>&& job, duration dt,
                              const char* filename, uint32_t linum)
{
    return schedule(std::move(job), clock::now() + dt,
                    filename, linum);
}

std::shared_ptr<RepeatedTask>
ScheduledExecutor::scheduleAtFixedRate(std::function<bool()>&& job,
                                       duration dt,
                                       const char* filename, uint32_t linum)
{
    auto ret = std::make_shared<RepeatedTask>(std::move(job), filename, linum);
    reschedule(ret, clock::now(), dt);
    return ret;
}

void
ScheduledExecutor::reschedule(std::shared_ptr<RepeatedTask> task, time_point t, duration dt)
{
    const char* filename =  task->job().filename;
    uint32_t linenum = task->job().linum;
    schedule(std::make_shared<Task>([this, task = std::move(task), t, dt]() mutable {
        if (task->run(name_.c_str()))
                reschedule(std::move(task), t + dt, dt);
    }, filename, linenum),
             t);
}

void
ScheduledExecutor::schedule(std::shared_ptr<Task> task, time_point t)
{
    const char* filename =  task->job().filename;
    uint32_t linenum = task->job().linum;
    std::lock_guard<std::mutex> lock(jobLock_);
    jobs_[t].emplace_back([task = std::move(task), this] { task->run(name_.c_str()); },
                            filename, linenum);
    cv_.notify_all();
}

void
ScheduledExecutor::loop()
{
    std::vector<Job> jobs;
    {
        std::unique_lock<std::mutex> lock(jobLock_);
        while (*running_ and (jobs_.empty() or jobs_.begin()->first > clock::now())) {
            if (jobs_.empty())
                cv_.wait(lock);
            else {
                auto nextJob = jobs_.begin()->first;
                cv_.wait_until(lock, nextJob);
            }
        }
        if (not *running_)
            return;
        jobs = std::move(jobs_.begin()->second);
        jobs_.erase(jobs_.begin());
    }
    for (auto& job : jobs) {
        try {
            job.fn();
        } catch (const std::exception& e) {
            JAMI_ERR("Exception running job: %s", e.what());
        }
    }
}

} // namespace jami
