/*
 *  Copyright (C) 2013-2015 Savoir-Faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "threadloop.h"
#include "logger.h"

namespace ring {

void
ThreadLoop::mainloop(const std::function<bool()> setup,
                     const std::function<void()> process,
                     const std::function<void()> cleanup)
{
    try {
        if (setup()) {
            while (state_ == RUNNING)
                process();
            cleanup();
        } else {
            RING_ERR("setup failed");
        }
    } catch (const ThreadLoopException& e) {
        RING_ERR("%s", e.what());
    }
}

ThreadLoop::ThreadLoop(const std::function<bool()>& setup,
                       const std::function<void()>& process,
                       const std::function<void()>& cleanup)
    : setup_(setup)
    , process_(process)
    , cleanup_(cleanup)
    , thread_()
{}

ThreadLoop::ThreadLoop(ThreadLoop&& other)
    : setup_(std::move(other.setup_))
    , process_(std::move(other.process_))
    , cleanup_(std::move(other.cleanup_))
    , state_(other.state_.load())
    , thread_(std::move(other.thread_))
{
    other.state_ = READY;
}

ThreadLoop::~ThreadLoop()
{
    if (isRunning()) {
        RING_ERR("join() should be explicitly called in owner's destructor");
        join();
    }
}

void
ThreadLoop::start()
{
    const auto s = state_.load();

    if (s == RUNNING) {
        RING_ERR("already started");
        return;
    }

    // stop pending but not processed by thread yet?
    if (s == STOPPING and thread_.joinable()) {
        RING_DBG("stop pending");
        thread_.join();
    }

    state_ = RUNNING;
    thread_ = std::thread(&ThreadLoop::mainloop, this, setup_, process_, cleanup_);
}

void
ThreadLoop::stop()
{
    if (state_ == RUNNING)
        state_ = STOPPING;
}

void
ThreadLoop::join()
{
    stop();
    if (thread_.joinable())
        thread_.join();
}

void ThreadLoop::exit()
{
    stop();
    throw ThreadLoopException();
}

bool
ThreadLoop::isRunning() const noexcept
{
    return thread_.joinable() and state_ == RUNNING;
}

} // namespace ring
