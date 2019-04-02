/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

static pthread_mutex_t count_mutex;
static pthread_cond_t count_nb_thread;
static int counter = 0;

CPPUNIT_TEST_SUITE_REGISTRATION( test_SIP );

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
    std::string *command = (std::string *)(str);

    std::cout << "test_SIP: " << command << std::endl;

    // Set up the sipp instance in this thread in order to catch return value
    // 0: All calls were successful
    // 1: At least one call failed
    // 97: exit on internal command. Calls may have been processed
    // 99: Normal exit without calls processed
    // -1: Fatal error
    // -2: Fatal error binding a socket
    int i = system(command->c_str()); //c_str() retrieve the *char of the string

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
    eventLoop_ = RAIIThread(std::thread([this]{
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
                  << ", ret=" << ret
                  << '\n';
    Manager::instance().callFactory.clear();
}

void
test_SIP::testSimpleOutgoingIpCall()
{
    std::cout << ">>>> test simple outgoing IP call <<<< " << '\n';

    CPPUNIT_ASSERT(Manager::instance().callFactory.empty());

    // start a user agent server waiting for a call
    auto t = sippThread("sipp -sn uas -i 127.0.0.1 -p 5068 -m 1 -bg");

    std::string testaccount("IP2IP");
    std::string testcallnumber("sip:test@127.0.0.1:5068");
    std::string testcallid; // returned by outgoingCall()

    CPPUNIT_ASSERT(!Manager::instance().hasCurrentCall());

    // start a new call sending INVITE message to sipp instance
    testcallid = Manager::instance().outgoingCall(testaccount, testcallnumber);

    // wait for receiving 180 and 200 message from peer
    std::this_thread::sleep_for(std::chrono::seconds(1)); // should be enough

    auto call = Manager::instance().getCallFromCallID(testcallid);
    CPPUNIT_ASSERT(call);

    // check call state
    auto state = call->getStateStr();
    std::cout << ">>>> call state is now " << state << '\n';
    CPPUNIT_ASSERT(state == DRing::Call::StateEvent::CURRENT);

    // hangup call
    std::cout << ">>>> hangup the call " << '\n';
    Manager::instance().hangupCall(testcallid);
    state = call->getStateStr();
    std::cout << ">>>> call state is now " << state << '\n';
    CPPUNIT_ASSERT(state == DRing::Call::StateEvent::OVER);

    // Call must not not be available (except for thus how already own a pointer like us)
    CPPUNIT_ASSERT(not Manager::instance().getCallFromCallID(testcallid));
}


void
test_SIP::testSimpleIncomingIpCall()
{
    std::cout << ">>>> test simple incoming IP call <<<< " << '\n';

    CPPUNIT_ASSERT(Manager::instance().callFactory.empty());

    // command to be executed by the thread, user agent client which initiate a call and hangup
    sippThread("sipp -sn uac 127.0.0.1 -i 127.0.0.1 -p 5062 -m 1 -bg");

    // sleep a while to make sure that sipp insdtance is initialized and dring received
    // the incoming invite.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Answer this call
    const auto& calls = Manager::instance().callFactory.getAllCalls();
    const auto call = *calls.cbegin();
    CPPUNIT_ASSERT(Manager::instance().answerCall(call->getCallId()));

    // hangup this call
    Manager::instance().hangupCall(call->getCallId());
    auto state = call->getStateStr();
    std::cout << ">>>> call state is now " << state << '\n';
    CPPUNIT_ASSERT(state == DRing::Call::StateEvent::OVER);
}

void
test_SIP::testMultipleIncomingIpCall(){
    std::cout << ">>>> test multiple incoming IP call <<<< " << '\n';

    CPPUNIT_ASSERT(Manager::instance().callFactory.empty());

    // this value change the number of outgoing call we do
    int numberOfCall =5;

    for(int i = 0; i < numberOfCall; i++){
        // start a user agent server waiting for a call
        sippThread("sipp -sf sippxml/test_2.xml 127.0.0.1 -i 127.0.0.1 -p 506"+std::to_string(i+1)+" -m 1 -bg");

        // sleep a while to make sure that sipp insdtance is initialized and dring received
        // the incoming invite.
        std::this_thread::sleep_for(std::chrono::seconds(3));

        const auto& calls = Manager::instance().callFactory.getAllCalls();
        const auto call = *calls.cbegin();
        auto state = call->getStateStr();
        std::cout << ">>>> current call (call number: "+std::to_string(i)+") state:" << state << '\n';
        CPPUNIT_ASSERT(state == DRing::Call::StateEvent::INCOMING); //TODO this state is sometime HOLD

        // Answer this call
        CPPUNIT_ASSERT(Manager::instance().answerCall(call->getCallId()));

        state = call->getStateStr();
        std::cout << ">>>> current call (call number: "+std::to_string(i)+") state:" << state << '\n';
        CPPUNIT_ASSERT(state == DRing::Call::StateEvent::CURRENT);

        Manager::instance().onHoldCall(call->getCallId());
        state = call->getStateStr();
        std::cout << ">>>> current call (call number: "+std::to_string(i)+") state:" << state << '\n';
        CPPUNIT_ASSERT(state == DRing::Call::StateEvent::HOLD);
    }

}


void
test_SIP::testMultipleOutgoingIpCall()
{
    std::cout << ">>>> test multiple outgoing IP call <<<< " << '\n';

    CPPUNIT_ASSERT(Manager::instance().callFactory.empty());

    // this value change the number of outgoing call we do
    int numberOfCall =5;

    //setup of the calls
    std::string callNumber("sip:test@127.0.0.1:5061");
    std::string testaccount("IP2IP");
    std::string callID[numberOfCall];

    for(int i = 0; i < numberOfCall; i++){
        // start a user agent server waiting for a call
        sippThread("sipp -sn uas -i 127.0.0.1 -p 5061 -m "+std::to_string(numberOfCall)+" -bg");

        callID[i] = Manager::instance().outgoingCall(testaccount, callNumber);
        auto newCall = Manager::instance().getCallFromCallID(callID[i]);
        CPPUNIT_ASSERT(newCall);

        // wait for receiving 180 and 200 message from peer
        std::this_thread::sleep_for(std::chrono::seconds(1)); // should be enough

        auto state = newCall->getStateStr();
        std::cout << ">>>> current call (call number: "+std::to_string(i)+") state:" << state << '\n';
        CPPUNIT_ASSERT(state == DRing::Call::StateEvent::CURRENT);

        // test the changement of calls states after doing a new call
        if(i){
            for(int j = 0; j<i; j++){
                auto oldCall = Manager::instance().getCallFromCallID(callID[j]);
                CPPUNIT_ASSERT(oldCall);
                auto oldState = oldCall->getStateStr();
                std::cout << ">>>> old call (call number: "+std::to_string(j)+") state:" << oldState << '\n';
                CPPUNIT_ASSERT(oldState == DRing::Call::StateEvent::HOLD);
                }
            }
        }

    //hangup  all calls
    for(int i = 0; i < numberOfCall; i++){
        auto call = Manager::instance().getCallFromCallID(callID[i]);
        Manager::instance().hangupCall(callID[i]);
        auto state = call->getStateStr();
        CPPUNIT_ASSERT(state == DRing::Call::StateEvent::OVER);
    }
}

void
test_SIP::testHoldIpCall()
{
    std::cout << ">>>> test hold IP call <<<< " << '\n';

    CPPUNIT_ASSERT(Manager::instance().callFactory.empty());

    auto testAccount = "IP2IP";
    auto testCallNumber = "sip:test@127.0.0.1:5062";

    auto callThread = sippThread("sipp -sf sippxml/test_3.xml -i 127.0.0.1 -p 5062 -m 1 -bg");

    auto testCallId = Manager::instance().outgoingCall(testAccount, testCallNumber);
    auto call = Manager::instance().getCallFromCallID(testCallId);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto state = call->getStateStr();
    std::cout << ">>>> call state is now " << state << '\n';
    CPPUNIT_ASSERT(state == DRing::Call::StateEvent::CURRENT);

    Manager::instance().onHoldCall(testCallId);

    state = call->getStateStr();
    std::cout << ">>>> call state is now " << state << '\n';
    CPPUNIT_ASSERT(state == DRing::Call::StateEvent::HOLD);

    Manager::instance().offHoldCall(testCallId);

    state = call->getStateStr();
    std::cout << ">>>> call state is now " << state << '\n';
    CPPUNIT_ASSERT(state == DRing::Call::StateEvent::CURRENT);

    Manager::instance().hangupCall(testCallId);
    state = call->getStateStr();
    std::cout << ">>>> call state is now " << state << '\n';
    CPPUNIT_ASSERT(state == DRing::Call::StateEvent::OVER);
}


void test_SIP::testSIPURI()
{
    std::cout << ">>>> test SIPURI <<<< " << '\n';

    std::string foo("<sip:17771234567@callcentric.com>");
    sip_utils::stripSipUriPrefix(foo);
    CPPUNIT_ASSERT(foo == "17771234567");
}
