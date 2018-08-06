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
#include <thread>
#include <functional>
#include <map>
#include <vector>
#include <chrono>

namespace ring {

class ScheduledExecutor {
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = clock::duration;
    using Job = std::function<void()>;

    ScheduledExecutor();
    ~ScheduledExecutor();
    void run(Job&& job);
    void schedule(Job&& job, time_point t);
    void scheduleIn(Job&& job, duration dt);
    void stop();

private:
    void loop();
    std::atomic_bool running_ {true};
    std::map<time_point, std::vector<Job>> jobs_;
    std::thread thread_;
    std::mutex jobLock_;
    std::condition_variable cv_;
};

}
