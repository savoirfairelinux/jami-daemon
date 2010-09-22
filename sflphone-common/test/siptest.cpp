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

using std::cout;
using std::endl;

pthread_t thethread;





void *sippThread(void *threadid)
{

    std::cout << "SIPTest: Starting sipp" << std::endl;

    // Set up the sipp instance in this thread in order to catch return value 
    // 0: All calls were successful
    // 1: At least one call failed
    // 97: exit on internal command. Calls may have been processed
    // 99: Normal exit without calls processed
    // -1: Fatal error
    // -2: Fatal error binding a socket 
    int i = system("sipp -sn uas -i 127.0.0.1 -p 5062 -m 1 -f 0 -trace_msg -trace_stat");
	
    CPPUNIT_ASSERT(i==0);

    pthread_exit(NULL);

}


void SIPTest::setUp()
{

    /*
    int rc = pthread_create(&thethread, NULL, sippThread, NULL);
    if (rc) {
        std::cout << "SIPTest: ERROR; return code from pthread_create()" << std::endl;
    }
    */
}

void SIPTest::tearDown()
{
    /*

    void *status;

    int rc = pthread_join(thethread, &status);
    if (rc) {
        std::cout << "SIPTest: ERROR; return code from pthread_join(): " << rc << std::endl;
    }
    else
	std::cout << " SIPTest: completed join with thread" << std::endl;

    */
}


void SIPTest::testSimpleIpCall ()
{

    

    std::string testaccount("IP2IP");
    std::string testcallid("callid1234");
    std::string testcallnumber("sip:test@127.0.0.1:5062");

    CPPUNIT_ASSERT(!Manager::instance().hasCurrentCall());

    // start a new call sending INVITE message to sipp instance
    Manager::instance().outgoingCall(testaccount, testcallid, testcallnumber);

    // must sleep here until receiving 180 and 200 message from peer
    sleep(2);

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

}

