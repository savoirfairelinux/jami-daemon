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



/*
 * @file sdptest.h
 * @brief       Regroups unitary tests related to the SDP session
 */

#ifndef _SDP_TEST_
#define _SDP_TEST_

// Cppunit import
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>

#include <exception>
#include <string>

#include <pj/pool.h>
#include <pjmedia/sdp.h>
#include <pjmedia/sdp_neg.h>
#include <pjmedia/errno.h>
#include <pjsip/sip_transport.h>
#include <pjlib.h>
#include <pjsip_ua.h>

#include "global.h"
#include "sip/sdp.h"
#include "noncopyable.h"

class SdpSessionException : public std::exception {
    public:
        SdpSessionException(const std::string& str="") throw() : errstr(str) {}

        virtual ~SdpSessionException() throw() {}

        virtual const char *what() const throw() {
            std::string expt("SdpSession: SdpSessionException occured: ");
            expt.append(errstr);
            return expt.c_str();
        }
    private:
        std::string errstr;
};


class SDPTest : public CppUnit::TestCase {

        /**
          * Use cppunit library macros to add unit test the factory
          */
        CPPUNIT_TEST_SUITE(SDPTest);
        CPPUNIT_TEST(testInitialOfferLastCodec);
        CPPUNIT_TEST(testInitialAnswerLastCodec);
        CPPUNIT_TEST(testInitialOfferLastCodec);
        CPPUNIT_TEST(testInitialAnswerLastCodec);
        CPPUNIT_TEST(testReinvite);
        CPPUNIT_TEST_SUITE_END();

    public:
        SDPTest() : CppUnit::TestCase("SDP module Tests"), session_(0),
        testPool_(0), poolCache_() {}

        /**
         * Code factoring - Common resources can be initialized here.
         * This method is called by unitcpp before each test
         */
        void setUp();

        /**
         * Code factoring - Common resources can be released here.
         * This method is called by unitcpp after each test
         */
        void tearDown();

        void testInitialOfferFirstCodec();

        void testInitialAnswerFirstCodec();

        void testInitialOfferLastCodec();

        void testInitialAnswerLastCodec();

        void testReinvite();

    private:
        NON_COPYABLE(SDPTest);
        void receiveAnswerAfterInitialOffer(const pjmedia_sdp_session* remote);

        Sdp *session_;
        pj_pool_t *testPool_;
        pj_caching_pool poolCache_;
};

/* Register our test module */
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SDPTest, "SDPTest");
CPPUNIT_TEST_SUITE_REGISTRATION(SDPTest);

#endif
