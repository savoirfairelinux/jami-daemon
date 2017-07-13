/*
 *  Copyright (C) 2004-2017 Savoir-Faire Linux Inc.
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

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "../../test_runner.h"
#include "conference.h"

#include "dring.h"

#include <string>
#include <iostream>

namespace ring_test {
    class ConferenceTest : public CppUnit::TestFixture {
    public:
        static std::string name() { return "conference"; }

        void setUp();
        void tearDown();

    private:
        void testGetConfID();
        void testState();
        void testManageParticipants();
        void testToggleRecording();

        CPPUNIT_TEST_SUITE(ConferenceTest);
        CPPUNIT_TEST(testGetConfID);
        CPPUNIT_TEST(testState);
        CPPUNIT_TEST(testManageParticipants);
        CPPUNIT_TEST(testToggleRecording);
        CPPUNIT_TEST_SUITE_END();

        ring::Conference* conference;
    };

    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConferenceTest,
                                                ConferenceTest::name());

    void
    ConferenceTest::setUp()
    {
        conference = new ring::Conference();
    }

    void
    ConferenceTest::tearDown()
    {
        delete conference;
    }

    void
    ConferenceTest::testGetConfID()
    {

        std::cout<< "size: " << conference->getConfID().size() <<'\n';
        CPPUNIT_ASSERT(conference->getConfID().size()==19||20);
    }

    void
    ConferenceTest::testState()
    {
        CPPUNIT_ASSERT(conference->getState()==0);

        conference->setState(conference->ACTIVE_DETACHED);
        CPPUNIT_ASSERT(conference->getState()==1);
        CPPUNIT_ASSERT(conference->getStateStr()=="ACTIVE_DETACHED");

        conference->setState(conference->ACTIVE_ATTACHED_REC);
        CPPUNIT_ASSERT(conference->getState()==2);
        CPPUNIT_ASSERT(conference->getStateStr()=="ACTIVE_ATTACHED_REC");

        conference->setState(conference->ACTIVE_DETACHED_REC);
        CPPUNIT_ASSERT(conference->getState()==3);
        CPPUNIT_ASSERT(conference->getStateStr()=="ACTIVE_DETACHED_REC");

        conference->setState(conference->HOLD);
        CPPUNIT_ASSERT(conference->getState()==4);
        CPPUNIT_ASSERT(conference->getStateStr()=="HOLD");

        conference->setState(conference->HOLD_REC);
        CPPUNIT_ASSERT(conference->getState()==5);
        CPPUNIT_ASSERT(conference->getStateStr()=="HOLD_REC");
    }

    void
    ConferenceTest::testManageParticipants()
    {
        CPPUNIT_ASSERT(conference->getParticipantList().empty());

        int nbrParticipants = 5;

        for(int i=0; i<nbrParticipants; i++){
            conference->add("participant"+std::to_string(i+1));
            CPPUNIT_ASSERT(conference->getParticipantList().size() == i+1);
        }
        std::set<std::string> participants = conference->getParticipantList();

        int i = 0;
        while (!participants.empty()) {
            i++;
            std::string participant = *participants.begin();
            std::cout << "participant: " << participant << std::endl;
            CPPUNIT_ASSERT(participant.compare("participant"+std::to_string(i)+"F"));
            participants.erase(participants.begin());
        }

        for(int j=0; j<nbrParticipants; j++){
            conference->remove("participant"+std::to_string(j+1));
            CPPUNIT_ASSERT(conference->getParticipantList().size() == nbrParticipants-(j+1));
        }

        CPPUNIT_ASSERT(conference->getParticipantList().empty());
    }

    void
    ConferenceTest::testToggleRecording()
    {
        CPPUNIT_ASSERT(conference->toggleRecording());
        CPPUNIT_ASSERT(!conference->toggleRecording());
    }
} // namespace tests

RING_TEST_RUNNER(ring_test::ConferenceTest::name())
