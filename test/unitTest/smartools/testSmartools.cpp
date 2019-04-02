/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
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

#define TESTING
#include "smartools.h"
#include "../../test_runner.h"

namespace jami {
class SmartoolsTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "smartools"; }

private:
    void testSetLocalInformation();
    void testSetRemoteInformation();

    CPPUNIT_TEST_SUITE(SmartoolsTest);
    CPPUNIT_TEST(testSetLocalInformation);
    CPPUNIT_TEST(testSetRemoteInformation);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SmartoolsTest, SmartoolsTest::name());

template <typename Map>
bool map_compare (Map const &lhs, Map const &rhs) {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

void
SmartoolsTest::testSetLocalInformation()
{
    std::map<std::string, std::string> localInformation;

    // initialisation localInformation
    localInformation["local FPS"]= "32";
    localInformation["local width"]="326";
    localInformation["local height"] ="532";
    localInformation["remote audio codec"]= "remoteAudioCodec";
    localInformation["local audio codec"]= "localAudioCodec";
    localInformation["local video codec"]= "localVideoCodec";

    Smartools::getInstance().setFrameRate("local", "32");
    Smartools::getInstance().setResolution("local", 326, 532);
    Smartools::getInstance().setRemoteAudioCodec("remoteAudioCodec");
    Smartools::getInstance().setLocalAudioCodec("localAudioCodec");
    Smartools::getInstance().setLocalVideoCodec("localVideoCodec");

    CPPUNIT_ASSERT(map_compare(localInformation, Smartools::getInstance().information_));

    // Clear information_
    Smartools::getInstance().sendInfo();

    CPPUNIT_ASSERT(Smartools::getInstance().information_.empty());
}

void
SmartoolsTest::testSetRemoteInformation()
{
    std::map<std::string, std::string> remoteInformation;

    // initialisation remoteInformation
    remoteInformation["remote FPS"]= "42";
    remoteInformation["remote width"]="874";
    remoteInformation["remote height"] ="253";

    Smartools::getInstance().setFrameRate("remote", "42");
    Smartools::getInstance().setResolution("remote", 874, 253);

    CPPUNIT_ASSERT(map_compare(remoteInformation, Smartools::getInstance().information_));

    // Clear information_
    Smartools::getInstance().sendInfo();

    CPPUNIT_ASSERT(Smartools::getInstance().information_.empty());
}

} // namespace jami

RING_TEST_RUNNER(jami::SmartoolsTest::name())
