/*
 *  Copyright (C) 2022 Savoir-faire Linux Inc.
 *  Author: Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <filesystem>
#include <string>

#include "manager.h"
#include "plugin/jamipluginmanager.h"
#include "../../test_runner.h"
#include "jami.h"
#include "fileutils.h"
#include "account_const.h"

#include "common.h"

using namespace DRing::Account;

namespace jami {
namespace test {

class PluginsTest : public CppUnit::TestFixture
{
public:
    PluginsTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("jami-sample.yml"));
    }
    ~PluginsTest() { DRing::fini(); }
    static std::string name() { return "Plugins"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    std::string name_{};
    std::string jplPath_{};
    std::string installationPath_{};
    std::vector<std::string> handlers_{};

    void testEnable();
    void testInstallAndLoad();
    void testHandlers();
    void testDetailsAndPreferences();
    void testCall();
    void testMessage();

    CPPUNIT_TEST_SUITE(PluginsTest);
    CPPUNIT_TEST(testEnable);
    CPPUNIT_TEST(testInstallAndLoad);
    CPPUNIT_TEST(testHandlers);
    // CPPUNIT_TEST(testDetailsAndPreferences);
    // CPPUNIT_TEST(testCall);
    // CPPUNIT_TEST(testMessage);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(PluginsTest, PluginsTest::name());

void
PluginsTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];

    std::ifstream file = jami::fileutils::ifstream("plugins/plugin.yml");
    assert(file.is_open());
    YAML::Node node = YAML::Load(file);

    assert(node.IsMap());

    jplPath_ = node["jplDirectory"].as<std::string>();
    name_ = node["plugin"].as<std::string>();
    handlers_ = node["handlers"].as<std::vector<std::string>>();

    if (jplPath_.empty())
        jplPath_ = "plugins";
    jplPath_ = jplPath_ + DIR_SEPARATOR_CH + name_ + ".jpl";

    installationPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_CH + "plugins" + DIR_SEPARATOR_CH + name_;
}

void
PluginsTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId});
}

void
PluginsTest::testEnable()
{
    Manager::instance().pluginPreferences.setPluginsEnabled(true);
    CPPUNIT_ASSERT(Manager::instance().pluginPreferences.getPluginsEnabled());
    Manager::instance().pluginPreferences.setPluginsEnabled(false);
    CPPUNIT_ASSERT(!Manager::instance().pluginPreferences.getPluginsEnabled());
}

void
PluginsTest::testInstallAndLoad()
{
    Manager::instance().pluginPreferences.setPluginsEnabled(true);

    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().installPlugin(jplPath_, true));
    auto installedPlugins = Manager::instance().getJamiPluginManager().getInstalledPlugins();
    CPPUNIT_ASSERT(!installedPlugins.empty());
    CPPUNIT_ASSERT(std::find(installedPlugins.begin(),
                             installedPlugins.end(),
                             installationPath_)
                   != installedPlugins.end());

    auto loadedPlugins = Manager::instance().getJamiPluginManager().getLoadedPlugins();
    CPPUNIT_ASSERT(!loadedPlugins.empty());
    CPPUNIT_ASSERT(std::find(loadedPlugins.begin(),
                             loadedPlugins.end(),
                             installationPath_)
                   != loadedPlugins.end());

    CPPUNIT_ASSERT(Manager::instance().getJamiPluginManager().unloadPlugin(installationPath_));
    loadedPlugins = Manager::instance().getJamiPluginManager().getLoadedPlugins();
    CPPUNIT_ASSERT(std::find(loadedPlugins.begin(),
                             loadedPlugins.end(),
                             installationPath_)
                   == loadedPlugins.end());

    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().uninstallPlugin(installationPath_));
    installedPlugins = Manager::instance().getJamiPluginManager().getInstalledPlugins();
    CPPUNIT_ASSERT(std::find(installedPlugins.begin(),
                             installedPlugins.end(),
                             installationPath_)
                   == installedPlugins.end());

}

void
PluginsTest::testHandlers()
{
    Manager::instance().pluginPreferences.setPluginsEnabled(true);

    Manager::instance().getJamiPluginManager().installPlugin(jplPath_, true);

    auto mediaHandlers = Manager::instance().getJamiPluginManager().getCallServicesManager().getCallMediaHandlers();
    auto chatHandlers = Manager::instance().getJamiPluginManager().getChatServicesManager().getChatHandlers();

    auto handlerLoaded = handlers_.size(); // number of handlers expected
    for (auto handler : mediaHandlers)
    {
        auto details = Manager::instance().getJamiPluginManager().getCallServicesManager().getCallMediaHandlerDetails(handler);
        // check details expected for the test plugin
        if(std::find(handlers_.begin(),
                        handlers_.end(),
                        details["name"])
                   != handlers_.end()) {
            handlerLoaded--;
        }
    }
    for (auto handler : chatHandlers)
    {
        auto details = Manager::instance().getJamiPluginManager().getChatServicesManager().getChatHandlerDetails(handler);
        // check details expected for the test plugin
        if(std::find(handlers_.begin(),
                        handlers_.end(),
                        details["name"])
                   != handlers_.end()) {
            handlerLoaded--;
        }
    }

    CPPUNIT_ASSERT(!handlerLoaded); // All expected handlers were found
    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().uninstallPlugin(installationPath_));
}

void
PluginsTest::testDetailsAndPreferences()
{
    // Get-set

    // Get-set Translated

    // Get "Always"
}

void
PluginsTest::testCall()
{}

void
PluginsTest::testMessage()
{}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::PluginsTest::name())
