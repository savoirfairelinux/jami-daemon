/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
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

namespace ring {

ScheduledExecutor::ScheduledExecutor() : thread_([this]{
        while (running_.load())
            loop();
    })
{}

ScheduledExecutor::~ScheduledExecutor()
{
    running_ = false;
    cv_.notify_one();
    if (thread_.joinable())
        thread_.join();
}

void
ScheduledExecutor::stop()
{
    running_ = false;
    cv_.notify_one();
}

void
ScheduledExecutor::run(Job&& job)
{
    {
        std::lock_guard<std::mutex> lock(jobLock_);
        auto now = clock::now();
        jobs_[now].emplace_back(std::move(job));
    }
    cv_.notify_one();
}

void
ScheduledExecutor::schedule(Job&& job, time_point t)
{
    {
        std::lock_guard<std::mutex> lock(jobLock_);
        jobs_[t].emplace_back(std::move(job));
    }
    cv_.notify_one();
}

void
ScheduledExecutor::scheduleIn(Job&& job, duration dt)
{
    {
        std::lock_guard<std::mutex> lock(jobLock_);
        auto now = clock::now();
        jobs_[now + dt].emplace_back(std::move(job));
    }
    cv_.notify_one();
}

void
ScheduledExecutor::loop()
{
    std::vector<Job> jobs;
    {
        std::unique_lock<std::mutex> lock(jobLock_);
        if (jobs_.empty())
            cv_.wait(lock);
        else
            cv_.wait_until(lock, jobs_.begin()->first);
        if (not running_ or jobs_.empty() or jobs_.begin()->first > clock::now())
            return;
        jobs = std::move(jobs_.begin()->second);
        jobs_.erase(jobs_.begin());
    }
    for (auto& job : jobs)
        job();
}

}
