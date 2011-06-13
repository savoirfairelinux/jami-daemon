/*
 *  Copyright (C) 2011 Savoir-Faire Linux Inc.
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

#include "test_video_endpoint.h"
#include <cstdlib>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/ui/text/TextTestRunner.h>
#include "video_endpoint.h"

void VideoEndpointTest::testListCodecs()
{
    /* This would list codecs */
    typedef std::map<int, std::string> MapType;
    const MapType CODECS_MAP(sfl_video::getCodecsMap());
    int count = 0;
    for (MapType::const_iterator iter = CODECS_MAP.begin(); iter != CODECS_MAP.end(); ++iter)
        if (iter->second == "MP4V-ES")
            count++;

    CPPUNIT_ASSERT(count == 1);
}

int main (int argc, char* argv[])
{

    printf ("\nSFLphone Video Test Suite, by Savoir-Faire Linux 2011\n\n");

    // Default test suite : all tests
    std::string testSuiteName = "All Tests";


    // Get the top level suite from the registry
    printf ("\n\n=== Test Suite: %s ===\n\n", testSuiteName.c_str());
    CPPUNIT_NS::Test *suite = CPPUNIT_NS::TestFactoryRegistry::getRegistry(testSuiteName).makeTest();

    if (suite->getChildTestCount() == 0) {
        exit (EXIT_FAILURE);
    }

    // Adds the test to the list of test to run
    CppUnit::TextTestRunner runner;
    runner.addTest (suite);
    // Change the default outputter to a compiler error format outputter
    runner.setOutputter (new CppUnit::CompilerOutputter(&runner.result(), std::cerr));

    // Run the tests.
    bool wasSucessful = runner.run();

    // Return error code 1 if the one of test failed.
    return wasSucessful ? 0 : 1;
}
