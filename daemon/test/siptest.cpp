/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>

#include <pthread.h>
#include <string>

#include "siptest.h"
#include "manager.h"
#include "sip/sipvoiplink.h"

using std::cout;
using std::endl;

// anonymous namespace
namespace {
pthread_mutex_t count_mutex;
pthread_cond_t count_nb_thread;
int counter = 0;
}

void *sippThreadWithCount(void *str)
{
    pthread_mutex_lock(&count_mutex);
    counter++;
    pthread_mutex_unlock(&count_mutex);

    std::string *command = (std::string *)(str);

    std::cout << "SIPTest: " << command << std::endl;

    // Set up the sipp instance in this thread in order to catch return value
    // 0: All calls were successful
    // 1: At least one call failed
    // 97: exit on internal command. Calls may have been processed
    // 99: Normal exit without calls processed
    // -1: Fatal error
    // -2: Fatal error binding a socket
    int i = system(command->c_str());

    CPPUNIT_ASSERT(i!=0);

    pthread_mutex_lock(&count_mutex);
    counter--;

    if (counter == 0)
        pthread_cond_signal(&count_nb_thread);

    pthread_mutex_unlock(&count_mutex);

    pthread_exit(NULL);
}


void *sippThread(void *str)
{
    std::string *command = (std::string *)(str);

    std::cout << "SIPTest: " << command << std::endl;

    // Set up the sipp instance in this thread in order to catch return value
    // 0: All calls were successful
    // 1: At least one call failed
    // 97: exit on internal command. Calls may have been processed
    // 99: Normal exit without calls processed
    // -1: Fatal error
    // -2: Fatal error binding a socket
    int i = system(command->c_str());

    CPPUNIT_ASSERT(i==0);

    pthread_exit(NULL);
}


void SIPTest::setUp()
{
    pthread_mutex_lock(&count_mutex);
    counter = 0;
    pthread_mutex_unlock(&count_mutex);
}

void SIPTest::tearDown()
{

    // in order to stop any currently running threads
    std::cout << "SIPTest: Clean all remaining sipp instances" << std::endl;
    int ret = system("killall sipp");

    if (!ret)
        std::cout << "SIPTest: Error from system call, killall sipp" << std::endl;
}


void SIPTest::testSimpleOutgoingIpCall()
{
    pthread_t thethread;

    // command to be executed by the thread, user agent server waiting for a call
    std::string command("sipp -sn uas -i 127.0.0.1 -p 5062 -m 1 -bg");

    int rc = pthread_create(&thethread, NULL, sippThread, (void *)(&command));

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;

    std::string testaccount("IP2IP");
    std::string testcallid("callid1234");
    std::string testcallnumber("sip:test@127.0.0.1:5062");

    CPPUNIT_ASSERT(!Manager::instance().hasCurrentCall());

    // start a new call sending INVITE message to sipp instance
    Manager::instance().outgoingCall(testaccount, testcallid, testcallnumber);

    // must sleep here until receiving 180 and 200 message from peer
    sleep(2);

    // call list should be empty for outgoing calls, only used for incoming calls
    CPPUNIT_ASSERT(Manager::instance().getCallList().size() == 0);

    CPPUNIT_ASSERT(Manager::instance().hasCurrentCall());
    CPPUNIT_ASSERT(Manager::instance().getCurrentCallId() == testcallid);

    std::map<std::string, std::string>::iterator iterCallDetails;
    std::map<std::string, std::string> callDetails = Manager::instance().getCallDetails(testcallid);

    iterCallDetails = callDetails.find("ACCOUNTID");
    CPPUNIT_ASSERT((iterCallDetails != callDetails.end()) && (iterCallDetails->second == ""));
    iterCallDetails = callDetails.find("PEER_NUMBER");
    CPPUNIT_ASSERT((iterCallDetails != callDetails.end()) && (iterCallDetails->second == "<sip:test@127.0.0.1:5062>"));
    iterCallDetails = callDetails.find("PEER_NAME");
    CPPUNIT_ASSERT((iterCallDetails != callDetails.end()) && (iterCallDetails->second == ""));
    iterCallDetails = callDetails.find("DISPLAY_NAME");
    CPPUNIT_ASSERT((iterCallDetails != callDetails.end()) && (iterCallDetails->second == ""));
    iterCallDetails = callDetails.find("CALL_STATE");
    CPPUNIT_ASSERT((iterCallDetails != callDetails.end()) && (iterCallDetails->second == "CURRENT"));
    iterCallDetails = callDetails.find("CALL_TYPE");
    CPPUNIT_ASSERT((iterCallDetails != callDetails.end()) && (iterCallDetails->second == "1"));

    Manager::instance().hangupCall(testcallid);

    void *status;
    rc = pthread_join(thethread, &status);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_join(): " << rc << std::endl;
    else
        std::cout << "SIPTest: completed join with thread" << std::endl;
}


void SIPTest::testSimpleIncomingIpCall()
{
    pthread_t thethread;
    void *status;

    // command to be executed by the thread, user agent client which initiate a call and hangup
    std::string command("sipp -sn uac 127.0.0.1 -i 127.0.0.1 -p 5062 -m 1i -bg");

    int rc = pthread_create(&thethread, NULL, sippThread, (void *)(&command));

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;

    // sleep a while to make sure that sipp insdtance is initialized and sflphoned received
    // the incoming invite.
    sleep(2);

    // gtrab call id from sipvoiplink
    SIPVoIPLink *siplink = SIPVoIPLink::instance();

    CPPUNIT_ASSERT(siplink->callMap_.size() == 1);
    CallMap::iterator iterCallId = siplink->callMap_.begin();
    std::string testcallid = iterCallId->first;

    // TODO: hmmm, should IP2IP call be stored in call list....
    CPPUNIT_ASSERT(Manager::instance().getCallList().size() == 0);

    // Answer this call
    CPPUNIT_ASSERT(Manager::instance().answerCall(testcallid));

    sleep(1);

    rc = pthread_join(thethread, &status);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_join(): " << rc << std::endl;
    else
        std::cout << "SIPTest: completed join with thread" << std::endl;
}


void SIPTest::testTwoOutgoingIpCall()
{
    pthread_t firstCallThread, secondCallThread;
    void *status;

    // This scenario expect to be put on hold before hangup
    std::string firstCallCommand("sipp -sf tools/sippxml/test_1.xml -i 127.0.0.1 -p 5062 -m 1");

    // The second call uses the default user agent scenario
    std::string secondCallCommand("sipp -sn uas -i 127.0.0.1 -p 5064 -m 1");

    int rc = pthread_create(&firstCallThread, NULL, sippThread, (void *)(&firstCallCommand));

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;

    rc = pthread_create(&secondCallThread, NULL, sippThread, (void *)(&secondCallCommand));

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;

    sleep(1);

    std::string testAccount("IP2IP");

    std::string firstCallID("callid1234");
    std::string firstCallNumber("sip:test@127.0.0.1:5062");

    std::string secondCallID("callid2345");
    std::string secondCallNumber("sip:test@127.0.0.1:5064");

    CPPUNIT_ASSERT(!Manager::instance().hasCurrentCall());

    // start a new call sending INVITE message to sipp instance
    // this call should be put on hold when making the second call
    Manager::instance().outgoingCall(testAccount, firstCallID, firstCallNumber);

    // must sleep here until receiving 180 and 200 message from peer
    sleep(1);

    Manager::instance().outgoingCall(testAccount, secondCallID, secondCallNumber);

    sleep(1);

    Manager::instance().hangupCall(firstCallID);

    rc = pthread_join(firstCallThread, &status);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_join(): " << rc << std::endl;

    std::cout << "SIPTest: completed join with thread" << std::endl;

    Manager::instance().hangupCall(secondCallID);

    rc = pthread_join(secondCallThread, &status);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_join(): " << rc << std::endl;
    else
        std::cout << "SIPTest: completed join with thread" << std::endl;
}

void SIPTest::testTwoIncomingIpCall()
{
    pthread_mutex_init(&count_mutex, NULL);
    pthread_cond_init(&count_nb_thread, NULL);

    pthread_t firstCallThread, secondCallThread;

    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // the first call is supposed to be put on hold when answering teh second incoming call
    std::string firstCallCommand("sipp -sf tools/sippxml/test_2.xml 127.0.0.1 -i 127.0.0.1 -p 5064 -m 1 > testfile1.txt -bg");

    // command to be executed by the thread, user agent client which initiate a call and hangup
    std::string secondCallCommand("sipp -sn uac 127.0.0.1 -i 127.0.0.1 -p 5062 -m 1 -d 250 > testfile2.txt -bg");

    int rc = pthread_create(&firstCallThread, &attr, sippThreadWithCount, (void *)(&firstCallCommand));

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;

    // sleep a while to make sure that sipp insdtance is initialized and sflphoned received
    // the incoming invite.
    sleep(1);

    // gtrab call id from sipvoiplink
    SIPVoIPLink *sipLink = SIPVoIPLink::instance();

    CPPUNIT_ASSERT(sipLink->callMap_.size() == 1);
    CallMap::iterator iterCallId = sipLink->callMap_.begin();
    std::string firstCallID = iterCallId->first;

    // Answer this call
    CPPUNIT_ASSERT(Manager::instance().answerCall(firstCallID));

    sleep(1);

    rc = pthread_create(&secondCallThread, &attr, sippThread, (void *)(&secondCallCommand));

    if (rc) {
        std::cout << "SIPTest: Error; return  code from pthread_create()" << std::endl;
    }

    sleep(1);

    CPPUNIT_ASSERT(sipLink->callMap_.size() == 2);
    iterCallId = sipLink->callMap_.begin();

    if (iterCallId->first == firstCallID)
        iterCallId++;

    std::string secondCallID = iterCallId->first;

    CPPUNIT_ASSERT(Manager::instance().answerCall(secondCallID));

    sleep(2);

    pthread_mutex_lock(&count_mutex);

    while (counter > 0)
        pthread_cond_wait(&count_nb_thread, &count_mutex);

    pthread_mutex_unlock(&count_mutex);

    pthread_mutex_destroy(&count_mutex);
    pthread_cond_destroy(&count_nb_thread);
}


void SIPTest::testHoldIpCall()
{
    pthread_t callThread;

    std::string callCommand("sipp -sf tools/sippxml/test_3.xml -i 127.0.0.1 -p 5062 -m 1 -bg");

    int rc = pthread_create(&callThread, NULL, sippThread, (void *)(&callCommand));

    if (rc) {
        std::cout << "SIPTest: ERROR; return code from pthread_create(): " << rc << std::endl;
    } else
        std::cout << "SIPTest: completed thread creation" << std::endl;


    std::string testAccount("IP2IP");

    std::string testCallID("callid1234");
    std::string testCallNumber("sip:test@127.0.0.1:5062");

    Manager::instance().outgoingCall(testAccount, testCallID, testCallNumber);

    sleep(1);

    Manager::instance().onHoldCall(testCallID);

    sleep(1);

    Manager::instance().offHoldCall(testCallID);

    sleep(1);

    Manager::instance().hangupCall(testCallID);
}


void SIPTest::testIncomingIpCallSdp()
{
    pthread_t thethread;
    void *status;

    // command to be executed by the thread, user agent client which initiate a call and hangup
    std::string command("sipp -sf tools/sippxml/test_4.xml 127.0.0.1 -i 127.0.0.1 -p 5062 -m 1i -bg");

    int rc = pthread_create(&thethread, NULL, sippThread, (void *)(&command));

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;

    // sleep a while to make sure that sipp insdtance is initialized and sflphoned received
    // the incoming invite.
    sleep(2);

    // gtrab call id from sipvoiplink
    SIPVoIPLink *siplink = SIPVoIPLink::instance();

    CPPUNIT_ASSERT(siplink->callMap_.size() == 1);
    CallMap::iterator iterCallId = siplink->callMap_.begin();
    std::string testcallid = iterCallId->first;

    // TODO: hmmm, should IP2IP call be stored in call list....
    CPPUNIT_ASSERT(Manager::instance().getCallList().size() == 0);

    // Answer this call
    CPPUNIT_ASSERT(Manager::instance().answerCall(testcallid));


    sleep(1);

    rc = pthread_join(thethread, &status);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_join(): " << rc << std::endl;
    else
        std::cout << "SIPTest: completed join with thread" << std::endl;
}
