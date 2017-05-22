/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

/*
 * @file audiocodectest.h
 * @brief       For every available audio codec, encode a buffer, decode it,
 *              and analyze the signal to ensure that it's similar to what was
 *              encoded.
 */

#ifndef AUDIO_CODEC_TEST_
#define AUDIO_CODEC_TEST_

// Cppunit import
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>

class AudioCodecTest: public CppUnit::TestFixture {

        /*
         * Use cppunit library macros to add unit test the factory
         */
        CPPUNIT_TEST_SUITE(AudioCodecTest);
        CPPUNIT_TEST(testCodecs);
        CPPUNIT_TEST_SUITE_END();

        static const short frequency_ = 440;

    public:
        void testCodecs();
};
/* Register our test module */
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AudioCodecTest, "AudioCodecTest");
CPPUNIT_TEST_SUITE_REGISTRATION(AudioCodecTest);

#endif // AUDIO_CODEC_TEST_
