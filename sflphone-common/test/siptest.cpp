/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
    
}

void SIPTest::tearDown()
{

    // in order to stop any currently running threads
    std::cout << "SIPTest: Clean all remaining sipp instances" << std::endl;
    int ret = system("killall sipp");
    if(!ret)
	std::cout << "SIPTest: Error from system call, killall sipp" << std::endl;
}


void SIPTest::testSimpleOutgoingIpCall ()
{
    pthread_t thethread;
    void *status;

    // command to be executed by the thread, user agent server waiting for a call
    std::string command("sipp -sn uas -i 127.0.0.1 -p 5062 -m 1");

    int rc = pthread_create(&thethread, NULL, sippThread, (void *)(&command));
    if (rc) {
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;
    }
    
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
    std::map<std::string, std::string> callDetails = Manager::instance().getCallDetails (testcallid);
   
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

    rc = pthread_join(thethread, &status);
    if (rc) {
        std::cout << "SIPTest: ERROR; return code from pthread_join(): " << rc << std::endl;
    }
    else
        std::cout << "SIPTest: completed join with thread" << std::endl;


}


void SIPTest::testSimpleIncomingIpCall ()
{

    pthread_t thethread;
    void *status;

    // command to be executed by the thread, user agent client which initiate a call and hangup
    std::string command("sipp -sn uac 127.0.0.1 -i 127.0.0.1 -p 5062 -m 1");

    int rc = pthread_create(&thethread, NULL, sippThread, (void *)(&command));
    if (rc) {
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;
    }
    

    // sleep a while to make sure that sipp insdtance is initialized and sflphoned received
    // the incoming invite.
    sleep(2);

    // gtrab call id from sipvoiplink 
    SIPVoIPLink *siplink = SIPVoIPLink::instance (""); 

    CPPUNIT_ASSERT(siplink->_callMap.size() == 1);
    CallMap::iterator iterCallId = siplink->_callMap.begin();
    std::string testcallid = iterCallId->first;
  
    // TODO: hmmm, should IP2IP call be stored in call list....
    CPPUNIT_ASSERT(Manager::instance().getCallList().size() == 0);
 
    // Answer this call
    CPPUNIT_ASSERT(Manager::instance().answerCall(testcallid));
    
    // This is useless since manager is not aware of incoming IP2IP sip calls
    /*
    sleep(1);

    std::map<std::string, std::string>::iterator iterCallDetails;
    std::map<std::string, std::string> callDetails = Manager::instance().getCallDetails (testcallid);

    iterCallDetails = callDetails.find("ACCOUNTID");
    std::cout << "---------------------- " << iterCallDetails->second << std::endl;
    // CPPUNIT_ASSERT((iterCallDetails != callDetails.end()) && (iterCallDetails->second == ""));
    iterCallDetails = callDetails.find("PEER_NUMBER");
    std::cout << "--------------------- " << iterCallDetails->second << std::endl;
    // CPPUNIT_ASSERT((iterCallDetails != callDetails.end()) && (iterCallDetails->second == "<sip:test@127.0.0.1:5062>"));
    iterCallDetails = callDetails.find("PEER_NAME");
    std::cout << "--------------------- " << iterCallDetails->second << std::endl;
    // CPPUNIT_ASSERT((iterCallDetails != callDetails.end()) && (iterCallDetails->second == ""));
    iterCallDetails = callDetails.find("DISPLAY_NAME");
    std::cout << "-------------------- " << iterCallDetails->second << std::endl;
    // CPPUNIT_ASSERT((iterCallDetails != callDetails.end()) && (iterCallDetails->second == ""));
    iterCallDetails = callDetails.find("CALL_STATE");
    std::cout << "-------------------- " << iterCallDetails->second << std::endl;
    // CPPUNIT_ASSERT((iterCallDetails != callDetails.end()) && (iterCallDetails->second == "CURRENT"));
    iterCallDetails = callDetails.find("CALL_TYPE");
    std::cout << "-------------------- " << iterCallDetails->second << std::endl;
    // CPPUNIT_ASSERT((iterCallDetails != callDetails.end()) && (iterCallDetails->second == "1"));
    */


    sleep(1);

    rc = pthread_join(thethread, &status);
    if (rc) {
        std::cout << "SIPTest: ERROR; return code from pthread_join(): " << rc << std::endl;
    }
    else
        std::cout << "SIPTest: completed join with thread" << std::endl;
}
