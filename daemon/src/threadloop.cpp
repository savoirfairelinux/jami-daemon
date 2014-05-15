/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
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

void ThreadLoop::mainloop()
{
    try {
        if (setup_()) {
            while (running_)
                process_();
            cleanup_();
        } else {
            ERROR("setup failed");
        }
    } catch (const ThreadLoopException &e) {
        ERROR("%s", e.what());
    }
}

ThreadLoop::ThreadLoop(const std::function<bool()> &setup,
                       const std::function<void()> &process,
                       const std::function<void()> &cleanup)
    : setup_(setup), process_(process), cleanup_(cleanup)
{}

ThreadLoop::~ThreadLoop()
{
    if (isRunning()) {
        ERROR("join() should be explicitly called in owner's destructor");
        join();
    }
}

void ThreadLoop::start()
{
    if (!running_.exchange(true)) {
        // a previous stop() call may be pending
        if (thread_.joinable())
            thread_.join();
        thread_ = std::thread(&ThreadLoop::mainloop, this);
    } else {
        ERROR("Thread already started");
    }
}

void ThreadLoop::stop()
{
    running_ = false;
}

void ThreadLoop::join()
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

bool ThreadLoop::isRunning() const
{
    return running_;
}
