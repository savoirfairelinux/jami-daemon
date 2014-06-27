/*
 *  Copyright (C) 2012-2013 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#include "shm_sink.h"
#include "shm_src.h"
#include <thread>
#include <signal.h>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <atomic>
#include <cstring>
#include <cassert>

static std::atomic<bool> done(false);

static void
signal_handler(int /*sig*/)
{
    done = true;
}

static const char test_data[] = "abcdefghijklmnopqrstuvwxyz";

static void
sink_thread()
{
    sfl_video::SHMSink sink("bob");;
    if (!sink.start())
        return;
    std::vector<unsigned char> test_vec(test_data, test_data + sizeof(test_data) / sizeof(test_data[0]));

    while (!done) {
        sink.render(test_vec);
        usleep(1000);
    }
    sink.stop();
    std::cerr << std::endl;
    std::cerr << "Exitting sink thread" << std::endl;
}

static void
run_client()
{
    SHMSrc src("bob");;
    bool started = false;
    while (not done and not started) {
        sleep(1);
        if (src.start())
            started = true;
    }
    // we get here if the above loop was interupted by our signal handler
    if (!started)
        return;

    // initialize destination string to 0's
    std::vector<char> dest(sizeof(test_data), 0);
    const std::vector<char> test_data_str(test_data, test_data + sizeof(test_data));
    assert(test_data_str.size() == 27);
    assert(dest.size() == test_data_str.size());
    while (not done and dest != test_data_str) {
        src.render(dest.data(), dest.size());
        usleep(1000);
    }
    src.stop();
    std::cerr << "Got characters, exitting client process" << std::endl;
}

static void
run_daemon()
{
    std::thread bob(sink_thread);
    /* Wait for child process. */
    int status;
    int pid;
    if ((pid = wait(&status)) == -1) {
        perror("wait error");
    } else {
        // Check status.
        if (WIFSIGNALED(status) != 0)
            std::cout << "Child process ended because of signal " <<
                    WTERMSIG(status) << std::endl;
        else if (WIFEXITED(status) != 0)
            std::cout << "Child process ended normally; status = " <<
                    WEXITSTATUS(status) << std::endl;
        else
            std::cout << "Child process did not end normally" << std::endl;
    }
    std::cout << "Finished waiting for child" << std::endl;
    done = true;
    // wait for thread
    bob.join();
}
int main()
{
    signal(SIGINT, signal_handler);
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Failed to fork" << std::endl;
        return 1;
    } else if (pid == 0) {
        // child code only
        run_client();
    } else {
        // parent code only
        run_daemon();
    }
    return 0;
}
