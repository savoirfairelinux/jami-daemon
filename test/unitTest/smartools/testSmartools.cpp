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

#include "smartools.h"
#include "../../test_runner.h"

namespace ring_test {
    class SmartoolsTest : public CppUnit::TestFixture {
    public:
        static std::string name() { return "smartools"; }

    private:
        void setInformationTest();
        std::map<std::string, std::string> information_;

        CPPUNIT_TEST_SUITE(SmartoolsTest);
        CPPUNIT_TEST(setInformationTest);
        CPPUNIT_TEST_SUITE_END();

    };

    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SmartoolsTest, SmartoolsTest::name());

    template <typename Map>
    bool map_compare (Map const &lhs, Map const &rhs) {
        return lhs.size() == rhs.size()
            && std::equal(lhs.begin(), lhs.end(),
                        rhs.begin());
    }

    void SmartoolsTest::setInformationTest()
    {
        // initialisation information_
        information_["local FPS"]= "32";
        information_["local width"]="326";
        information_["local height"] ="532";
        information_["remote audio codec"]= "remoteAudioCodec";
        information_["local audio codec"]= "localAudioCodec";
        information_["local video codec"]= "localVideoCodec";

        ring::Smartools::getInstance().setFrameRate("local", "32");
        ring::Smartools::getInstance().setResolution("local", 326, 532);
        ring::Smartools::getInstance().setRemoteAudioCodec("remoteAudioCodec");
        ring::Smartools::getInstance().setLocalAudioCodec("localAudioCodec");
        ring::Smartools::getInstance().setLocalVideoCodec("localVideoCodec");
        //TODO setRemoteVideoCodec create a SIP call to have the callID

        CPPUNIT_ASSERT(map_compare(information_,ring::Smartools::getInstance().getInformation()));

        //clear maps
        ring::Smartools::getInstance().sendInfo();
        information_.clear();

        // initialisation information_
        information_["remote FPS"]= "23";
        information_["remote width"]="678";
        information_["remote height"] ="987";

        ring::Smartools::getInstance().setFrameRate("remote", "23");
        ring::Smartools::getInstance().setResolution("remote", 678, 987);

        CPPUNIT_ASSERT(map_compare(information_,ring::Smartools::getInstance().getInformation()));
    }

} // namespace tests

RING_TEST_RUNNER(ring_test::SmartoolsTest::name())
