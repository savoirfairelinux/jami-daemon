/*
 *  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
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

ScheduledExecutor::ScheduledExecutor() : thread_([this]{
        while (running_.load())
            loop();
    })
{}

ScheduledExecutor::~ScheduledExecutor()
{
    stop();
    if (thread_.joinable())
        thread_.join();
}

void
ScheduledExecutor::stop()
{
    {
        std::lock_guard<std::mutex> lock(jobLock_);
        running_ = false;
        jobs_.clear();
    }
    cv_.notify_all();
}

void
ScheduledExecutor::run(Job&& job)
{
    {
        std::lock_guard<std::mutex> lock(jobLock_);
        auto now = clock::now();
        jobs_[now].emplace_back(std::move(job));
    }
    cv_.notify_all();
}

std::shared_ptr<Task>
ScheduledExecutor::schedule(Job&& job, time_point t)
{
    auto ret = std::make_shared<Task>(std::move(job));
    schedule(ret, t);
    return ret;
}

std::shared_ptr<Task>
ScheduledExecutor::scheduleIn(Job&& job, duration dt)
{
    return schedule(std::move(job), clock::now() + dt);
}

std::shared_ptr<RepeatedTask>
ScheduledExecutor::scheduleAtFixedRate(RepeatedJob&& job, duration dt)
{
    auto ret = std::make_shared<RepeatedTask>(std::move(job));
    reschedule(ret, clock::now(), dt);
    return ret;
}

void
ScheduledExecutor::reschedule(std::shared_ptr<RepeatedTask> task, time_point t, duration dt)
{
    schedule(std::make_shared<Task>([this, task = std::move(task), t, dt]() mutable {
        if (task->run())
            reschedule(std::move(task), t + dt, dt);
    }), t);
}

void
ScheduledExecutor::schedule(std::shared_ptr<Task> task, time_point t)
{
    {
        std::lock_guard<std::mutex> lock(jobLock_);
        jobs_[t].emplace_back([task = std::move(task)]{
            task->run();
        });
    }
    cv_.notify_all();
}

void
ScheduledExecutor::loop()
{
    std::vector<Job> jobs;
    {
        std::unique_lock<std::mutex> lock(jobLock_);
        while (running_ and (jobs_.empty() or jobs_.begin()->first > clock::now())) {
            if (jobs_.empty())
                cv_.wait(lock);
            else
                cv_.wait_until(lock, jobs_.begin()->first);
        }
        if (not running_)
            return;
        jobs = std::move(jobs_.begin()->second);
        jobs_.erase(jobs_.begin());
    }
    for (auto& job : jobs) {
        try {
            job();
        } catch (const std::exception& e) {
            JAMI_ERR("Exception running job: %s", e.what());
        }
    }
}

}
