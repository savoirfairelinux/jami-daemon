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

#ifndef __THREADLOOP_H__
#define __THREADLOOP_H__

#include <atomic>
#include <thread>
#include <functional>
#include <stdexcept>

// FIXME: this is ugly
// If condition A is false, print the error message in M and exit thread
#define EXIT_IF_FAIL(A, M, ...) if (!(A)) { \
        ERROR(M, ##__VA_ARGS__); loop_.exit(); }

struct ThreadLoopException : public std::runtime_error {
    ThreadLoopException() : std::runtime_error("ThreadLoopException") {}
};

class ThreadLoop {
public:
    ThreadLoop(const std::function<bool()> &setup,
               const std::function<void()> &process,
               const std::function<void()> &cleanup);
    ~ThreadLoop();

    void start();

    void exit();
    void stop();
    void join();
    bool isRunning() const;

private:
    // These must be provided by users of ThreadLoop
    std::function<bool()> setup_;
    std::function<void()> process_;
    std::function<void()> cleanup_;

    void mainloop();

    std::atomic<bool> running_ = {false};
    std::thread thread_ = {};
};

#endif // __THREADLOOP_H__
