/*
 *  Copyright (C) 2017 Savoir-Faire Linux Inc.
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

#include <string>
#include <vector>
#include "dring.h"

#include "preferences.h"
#include "../../test_runner.h"

#include <yaml-cpp/yaml.h>
#include "config/yamlparser.h"

// VideoPreferences
#include "client/videomanager.h"
#include "video/video_scaler.h"

namespace ring { namespace test {
    class PreferencesTest : public CppUnit::TestFixture {
        public:
            static std::string name() { return "preferences"; }
            void setUp();

        private:
            void testSerialize();
            void testManageAccount();
            void testUnserialize(); //WIP
            void init_daemon();

            CPPUNIT_TEST_SUITE(PreferencesTest);
            CPPUNIT_TEST(init_daemon);
            CPPUNIT_TEST(testSerialize);
            //CPPUNIT_TEST(testUnserialize);
            CPPUNIT_TEST(testManageAccount);
            CPPUNIT_TEST_SUITE_END();

            std::unique_ptr<Preferences> preferences;
            std::unique_ptr<VoipPreference> voipPreference;
            std::unique_ptr<HookPreference> hookPreference;
            std::unique_ptr<AudioPreference> audioPreference;
            std::unique_ptr<ShortcutPreferences> shortcutPreferences;
            //std::unique_ptr<VideoPreferences> videoPreferences;
    };

    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(PreferencesTest, PreferencesTest::name());

    void
    PreferencesTest::setUp()
    {
        preferences.reset(new Preferences());
        voipPreference.reset(new VoipPreference());
        hookPreference.reset(new HookPreference());
        audioPreference.reset(new AudioPreference());
        shortcutPreferences.reset(new ShortcutPreferences());
        //videoPreferences.reset(new VideoPreferences());
    }

    void
    PreferencesTest::init_daemon()
    {
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }

    void
    PreferencesTest::testManageAccount()
    {
        // Add accounts to accountOrder_
        std::string id("ID1");
        std::string id2("ID2");
        preferences->addAccount(id);
        CPPUNIT_ASSERT(preferences->getAccountOrder().compare(id+"/")==0);
        preferences->addAccount(id2);
        CPPUNIT_ASSERT(preferences->getAccountOrder().compare(id2+"/"+id+"/")==0);

        // Test verifyAccountOrder
        std::vector<std::string> accounts;
        accounts.push_back(id2);
        accounts.push_back("wrongAccountID");
        preferences->verifyAccountOrder(accounts);
        CPPUNIT_ASSERT(preferences->getAccountOrder().compare(id2+"/")==0);

        preferences->addAccount(id);
        CPPUNIT_ASSERT(preferences->getAccountOrder().compare(id+"/"+id2+"/")==0);

        // Remove accounts to accountOrder_
        preferences->removeAccount(id);
        CPPUNIT_ASSERT(preferences->getAccountOrder().compare(id2+"/")==0);
        preferences->removeAccount(id2);
        CPPUNIT_ASSERT(preferences->getAccountOrder().empty());
    }

    void
    PreferencesTest::testSerialize()
    {
        int x = 42;
        YAML::Emitter out;
        out << YAML::BeginMap << YAML::Key << "accounts";
        out << YAML::Value << YAML::BeginSeq;
        out << YAML::Value << x;
        out << YAML::EndSeq;
        preferences->serialize(out);
        voipPreference->serialize(out);
        hookPreference->serialize(out);
        audioPreference->serialize(out);
        shortcutPreferences->serialize(out);
        //videoPreferences->serialize(out);
    }

    void
    PreferencesTest::testUnserialize()
    {
        YAML::Node *node = new YAML::Node();



        //preferences->unserialize(*node);
        voipPreference->unserialize(*node);


    }

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::PreferencesTest::name())
