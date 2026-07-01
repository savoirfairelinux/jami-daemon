/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <yaml-cpp/yaml.h>

#include "jami.h"
#include "preferences.h"

#include "../../../test_runner.h"

namespace jami {
namespace test {

class VideoPreferencesTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "video_preferences"; }

    void setUp();
    void tearDown();

private:
    void testDefaultIsAuto();
    void testMigrateLegacyDefault();
    void testExplicitResolutionPreserved();
    void testMissingKeyFallsBackToDefault();
    void testSerializeRoundTrip();

    CPPUNIT_TEST_SUITE(VideoPreferencesTest);
    CPPUNIT_TEST(testDefaultIsAuto);
    CPPUNIT_TEST(testMigrateLegacyDefault);
    CPPUNIT_TEST(testExplicitResolutionPreserved);
    CPPUNIT_TEST(testMissingKeyFallsBackToDefault);
    CPPUNIT_TEST(testSerializeRoundTrip);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(VideoPreferencesTest, VideoPreferencesTest::name());

void
VideoPreferencesTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
}

void
VideoPreferencesTest::tearDown()
{
    libjami::fini();
}

void
VideoPreferencesTest::testDefaultIsAuto()
{
    VideoPreferences prefs;
    CPPUNIT_ASSERT_EQUAL(std::string("auto"), prefs.getConferenceResolution());
}

void
VideoPreferencesTest::testMigrateLegacyDefault()
{
    // "1280x720" was the previous fixed default and the preference is not
    // exposed through any client API: it must be migrated to "auto".
    YAML::Node root;
    root["video"]["conferenceResolution"] = "1280x720";

    VideoPreferences prefs;
    prefs.unserialize(root);
    CPPUNIT_ASSERT_EQUAL(std::string("auto"), prefs.getConferenceResolution());
}

void
VideoPreferencesTest::testExplicitResolutionPreserved()
{
    YAML::Node root;
    root["video"]["conferenceResolution"] = "1920x1080";

    VideoPreferences prefs;
    prefs.unserialize(root);
    CPPUNIT_ASSERT_EQUAL(std::string("1920x1080"), prefs.getConferenceResolution());
}

void
VideoPreferencesTest::testMissingKeyFallsBackToDefault()
{
    YAML::Node root;
    root["video"]["recordPreview"] = true;

    VideoPreferences prefs;
    prefs.unserialize(root);
    CPPUNIT_ASSERT_EQUAL(std::string("auto"), prefs.getConferenceResolution());
}

void
VideoPreferencesTest::testSerializeRoundTrip()
{
    VideoPreferences prefs;
    prefs.setConferenceResolution("2560x1440");

    YAML::Emitter out;
    out << YAML::BeginMap;
    prefs.serialize(out);
    out << YAML::EndMap;

    VideoPreferences reloaded;
    reloaded.unserialize(YAML::Load(out.c_str()));
    CPPUNIT_ASSERT_EQUAL(std::string("2560x1440"), reloaded.getConferenceResolution());
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::VideoPreferencesTest::name());
