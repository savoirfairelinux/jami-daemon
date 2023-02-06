/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Olivier Gregoire <olivier.gregoire@savoirfairelinux.com>
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

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>

#include <pthread.h>
#include <string>
#include <thread>

#include "test_SIP.h"
#include "call_const.h"

using namespace jami;
using namespace std::literals;

static pthread_mutex_t count_mutex;
static pthread_cond_t count_nb_thread;
static int counter = 0;

CPPUNIT_TEST_SUITE_REGISTRATION(test_SIP);

/*
return an error if all call are not successful
*/
void*
sippThreadWithCount(void* str)
{
    // number of time we use the mutex. Lock the utilisation of counter
    pthread_mutex_lock(&count_mutex);
    counter++;
    pthread_mutex_unlock(&count_mutex);

    // display what is send on the parameter of the method
    std::string* command = (std::string*) (str);

    std::cout << "test_SIP: " << command << std::endl;

    // Set up the sipp instance in this thread in order to catch return value
    // 0: All calls were successful
    // 1: At least one call failed
    // 97: exit on internal command. Calls may have been processed
    // 99: Normal exit without calls processed
    // -1: Fatal error
    // -2: Fatal error binding a socket
    int i = system(command->c_str()); // c_str() retrieve the *char of the string

    CPPUNIT_ASSERT(i);

    pthread_mutex_lock(&count_mutex);
    counter--;
    // ???
    if (counter == 0)
        pthread_cond_signal(&count_nb_thread);

    pthread_mutex_unlock(&count_mutex);

    pthread_exit(NULL);
}

RAIIThread
sippThread(const std::string& command)
{
    return std::thread([command] {
        std::cout << "test_SIP: " << command << std::endl;

        // Set up the sipp instance in this thread in order to catch return value
        // 0: All calls were successful
        // 1: At least one call failed
        // 97: exit on internal command. Calls may have been processed
        // 99: Normal exit without calls processed
        // -1: Fatal error
        // -2: Fatal error binding a socket
        auto ret = system(command.c_str());
        std::cout << "test_SIP: Command executed by system returned: " << ret << std::endl;
    });
}

void
test_SIP::setUp()
{
    std::cout << "setup test SIP" << std::endl;
    pthread_mutex_lock(&count_mutex);
    counter = 0;
    pthread_mutex_unlock(&count_mutex);

    running_ = true;
    eventLoop_ = RAIIThread(std::thread([this] {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }));
}

void
test_SIP::tearDown()
{
    running_ = false;
    eventLoop_.join();

    // in order to stop any currently running threads
    std::cout << "test_SIP: Clean all remaining sipp instances" << std::endl;
    int ret = system("killall sipp");
    if (ret)
        std::cout << "test_SIP: Error from system call, killall sipp"
                  << ", ret=" << ret << '\n';
    Manager::instance().callFactory.clear();
}
void
test_SIP::testSIPURI()
{
    std::cout << ">>>> test SIPURI <<<< " << '\n';

    auto foo = sip_utils::stripSipUriPrefix("<sip:17771234567@callcentric.com>"sv);
    CPPUNIT_ASSERT_EQUAL("17771234567"sv, foo);
}
