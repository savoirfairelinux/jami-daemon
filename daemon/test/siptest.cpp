/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>

#include <pthread.h>
#include <string>

#include "siptest.h"
#include "manager.h"
#include "sip/sipvoiplink.h"
#include "sip/sip_utils.h"

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

    CPPUNIT_ASSERT(i);

    pthread_mutex_lock(&count_mutex);
    counter--;

    if (counter == 0)
        pthread_cond_signal(&count_nb_thread);

    pthread_mutex_unlock(&count_mutex);

    pthread_exit(NULL);
}


void *sippThread(void *str)
{
    std::string *command = static_cast<std::string *>(str);
    std::cout << "SIPTest: " << command << std::endl;

    // Set up the sipp instance in this thread in order to catch return value
    // 0: All calls were successful
    // 1: At least one call failed
    // 97: exit on internal command. Calls may have been processed
    // 99: Normal exit without calls processed
    // -1: Fatal error
    // -2: Fatal error binding a socket
    int i = system(command->c_str());

    std::stringstream output;
    output << i;

    std::cout << "SIPTest: Command executed by system returned: " << output.str() << std::endl;
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
    std::string command("sipp -sn uas -i 127.0.0.1 -p 5068 -m 1 -bg");

    int rc = pthread_create(&thethread, NULL, sippThread, &command);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;

    std::string testaccount("IP2IP");
    std::string testcallid("callid1234");
    std::string testcallnumber("sip:test@127.0.0.1:5068");

    CPPUNIT_ASSERT(!Manager::instance().hasCurrentCall());

    // start a new call sending INVITE message to sipp instance
    Manager::instance().outgoingCall(testaccount, testcallid, testcallnumber);

    // must sleep here until receiving 180 and 200 message from peer
    sleep(2);

    CPPUNIT_ASSERT(Manager::instance().hasCurrentCall());
    CPPUNIT_ASSERT(Manager::instance().getCurrentCallId() == testcallid);

    Manager::instance().hangupCall(testcallid);

    rc = pthread_join(thethread, NULL);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_join(): " << rc << std::endl;
    else
        std::cout << "SIPTest: completed join with thread" << std::endl;
}


void SIPTest::testSimpleIncomingIpCall()
{
    pthread_t thethread;

    // command to be executed by the thread, user agent client which initiate a call and hangup
    std::string command("sipp -sn uac 127.0.0.1 -i 127.0.0.1 -p 5062 -m 1i -bg");

    int rc = pthread_create(&thethread, NULL, sippThread, &command);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;

    // sleep a while to make sure that sipp insdtance is initialized and sflphoned received
    // the incoming invite.
    sleep(2);

    // gtrab call id from sipvoiplink
    SIPVoIPLink& siplink = SIPVoIPLink::instance();

    CPPUNIT_ASSERT(siplink.sipCallMap_.size() == 1);
    SipCallMap::iterator iterCallId = siplink.sipCallMap_.begin();
    std::string testcallid = iterCallId->first;

    // Answer this call
    CPPUNIT_ASSERT(Manager::instance().answerCall(testcallid));

    sleep(1);

    rc = pthread_join(thethread, NULL);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_join(): " << rc << std::endl;
    else
        std::cout << "SIPTest: completed join with thread" << std::endl;
}


void SIPTest::testTwoOutgoingIpCall()
{
    // This scenario expect to be put on hold before hangup
    std::string firstCallCommand("sipp -sf tools/sippxml/test_1.xml -i 127.0.0.1 -p 5062 -m 1");

    // The second call uses the default user agent scenario
    std::string secondCallCommand("sipp -sn uas -i 127.0.0.1 -p 5064 -m 1");

    pthread_t firstCallThread;
    int rc = pthread_create(&firstCallThread, NULL, sippThread, &firstCallCommand);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;

    pthread_t secondCallThread;
    rc = pthread_create(&secondCallThread, NULL, sippThread, &secondCallCommand);

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

    rc = pthread_join(firstCallThread, NULL);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_join(): " << rc << std::endl;

    std::cout << "SIPTest: completed join with thread" << std::endl;

    Manager::instance().hangupCall(secondCallID);

    rc = pthread_join(secondCallThread, NULL);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_join(): " << rc << std::endl;
    else
        std::cout << "SIPTest: completed join with thread" << std::endl;
}

void SIPTest::testTwoIncomingIpCall()
{
    pthread_mutex_init(&count_mutex, NULL);
    pthread_cond_init(&count_nb_thread, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // the first call is supposed to be put on hold when answering teh second incoming call
    std::string firstCallCommand("sipp -sf tools/sippxml/test_2.xml 127.0.0.1 -i 127.0.0.1 -p 5064 -m 1 > testfile1.txt -bg");

    // command to be executed by the thread, user agent client which initiate a call and hangup
    std::string secondCallCommand("sipp -sn uac 127.0.0.1 -i 127.0.0.1 -p 5062 -m 1 -d 250 > testfile2.txt -bg");

    pthread_t firstCallThread;
    int rc = pthread_create(&firstCallThread, &attr, sippThreadWithCount, &firstCallCommand);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;

    // sleep a while to make sure that sipp insdtance is initialized and sflphoned received
    // the incoming invite.
    sleep(1);

    // gtrab call id from sipvoiplink
    SIPVoIPLink& sipLink = SIPVoIPLink::instance();

    CPPUNIT_ASSERT(sipLink.sipCallMap_.size() == 1);
    SipCallMap::iterator iterCallId = sipLink.sipCallMap_.begin();
    std::string firstCallID = iterCallId->first;

    // Answer this call
    CPPUNIT_ASSERT(Manager::instance().answerCall(firstCallID));

    sleep(1);
    pthread_t secondCallThread;
    rc = pthread_create(&secondCallThread, &attr, sippThread, &secondCallCommand);

    if (rc)
        std::cout << "SIPTest: Error; return  code from pthread_create()" << std::endl;

    sleep(1);

    CPPUNIT_ASSERT(sipLink.sipCallMap_.size() == 2);
    iterCallId = sipLink.sipCallMap_.begin();

    if (iterCallId->first == firstCallID)
        ++iterCallId;

    std::string secondCallID(iterCallId->first);

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
    std::string callCommand("sipp -sf tools/sippxml/test_3.xml -i 127.0.0.1 -p 5062 -m 1 -bg");

    pthread_t callThread;
    int rc = pthread_create(&callThread, NULL, sippThread, (void *)(&callCommand));

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_create(): " << rc << std::endl;
    else
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

void SIPTest::testSIPURI()
{
    std::string foo("<sip:17771234567@callcentric.com>");
    sip_utils::stripSipUriPrefix(foo);
    CPPUNIT_ASSERT(foo == "17771234567");
}

void SIPTest::testParseDisplayName()
{
    // 1st element is input, 2nd is expected output
    const char *test_set[][2] = {
    {"\nFrom: \"A. G. Bell\" <sip:agb@bell-telephone.com> ;tag=a48s", "A. G. Bell"},
    {"\nFrom: \"A. G. Bell2\" <sip:agb@bell-telephone.com> ;tag=a48s\r\nOtherLine: \"bla\"\r\n", "A. G. Bell2"},
    {"\nf: Anonymous <sip:c8oqz84zk7z@privacy.org>;tag=hyh8", "Anonymous"},
    {"\nFrom: \"Alejandro Perez\" <sip:1111@10.0.0.1>;tag=3a7516a63bdbo0", "Alejandro Perez"},
    {"\nFrom: \"Malformed <sip:1111@10.0.0.1>;tag=3a6a63bdbo0", ""},
    {"\nTo: <sip:1955@10.0.0.1>;tag=as6fbade41", ""},
    {"\nFrom: \"1000\" <sip:1000@sip.example.es>;tag=as775338f3", "1000"},
    {"\nFrom: 1111_9532323 <sip:1111_9532323@sip.example.es>;tag=caa3a61", "1111_9532323"},
    {"\nFrom: \"4444_953111111\" <sip:4444_111111@sip.example.es>;tag=2b00632co0", "4444_953111111"},
    {"\nFrom: <sip:6926666@4.4.4.4>;tag=4421-D9700", ""},
    {"\nFrom: <sip:pinger@sipwise.local>;tag=01f516a4", ""},
    {"\nFrom: sip:pinger@sipwise.local;tag=01f516a4", ""},
    {"\nFrom: ", ""},
    {"\nFrom: \"\xb1""Alejandro P\xc3\xa9rez\" <sip:1111@10.0.0.1>;tag=3a7516a63bdbo0", "\xef\xbf\xbd""Alejandro P\xc3\xa9rez"},
    {"\nFrom: \"Alejandro P\xc3\xa9rez\" <sip:1111@10.0.0.1>;tag=3a7516a63bdbo0", "Alejandro P\xc3\xa9rez"},
    {"\nFrom: sip:+1212555@server.example.com;tag=887s", ""}};

    for (const auto &t : test_set) {
        const std::string str(sip_utils::parseDisplayName(t[0]));
        CPPUNIT_ASSERT_MESSAGE(std::string("\"") + str + "\" should be \"" +
                               t[1] + "\", input on next line: " + t[0],
                               str == t[1]);
    }
}

void SIPTest::testIncomingIpCallSdp()
{
    // command to be executed by the thread, user agent client which initiate a call and hangup
    std::string command("sipp -sf tools/sippxml/test_4.xml 127.0.0.1 -i 127.0.0.1 -p 5062 -m 1i -bg");

    pthread_t thethread;
    int rc = pthread_create(&thethread, NULL, sippThread, (void *)(&command));

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;

    // sleep a while to make sure that sipp insdtance is initialized and sflphoned received
    // the incoming invite.
    sleep(2);

    // gtrab call id from sipvoiplink
    SIPVoIPLink& siplink = SIPVoIPLink::instance();

    CPPUNIT_ASSERT(siplink.sipCallMap_.size() == 1);
    SipCallMap::iterator iterCallId = siplink.sipCallMap_.begin();
    std::string testcallid = iterCallId->first;

    // TODO: hmmm, should IP2IP call be stored in call list....
    CPPUNIT_ASSERT(Manager::instance().getCallList().empty());

    // Answer this call
    CPPUNIT_ASSERT(Manager::instance().answerCall(testcallid));


    sleep(1);

    rc = pthread_join(thethread, NULL);

    if (rc)
        std::cout << "SIPTest: ERROR; return code from pthread_join(): " << rc << std::endl;
    else
        std::cout << "SIPTest: completed join with thread" << std::endl;
}
