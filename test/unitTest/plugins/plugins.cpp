/*
 *  Copyright (C) 2022-2023 Savoir-faire Linux Inc.
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
#include <opendht/crypto.h>
#include <filesystem>
#include <memory>
#include <string>

#include "manager.h"
#include "plugin/jamipluginmanager.h"
#include "plugin/pluginsutils.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "jami.h"
#include "fileutils.h"
#include "jami/media_const.h"
#include "account_const.h"
#include "sip/sipcall.h"
#include "call_const.h"

#include "common.h"

#include <yaml-cpp/yaml.h>

using namespace libjami::Account;

namespace jami {
namespace test {

struct CallData
{
    struct Signal
    {
        Signal(const std::string& name, const std::string& event = {})
            : name_(std::move(name))
            , event_(std::move(event)) {};

        std::string name_ {};
        std::string event_ {};
    };

    CallData() = default;
    CallData(CallData&& other) = delete;
    CallData(const CallData& other)
    {
        accountId_ = std::move(other.accountId_);
        listeningPort_ = other.listeningPort_;
        userName_ = std::move(other.userName_);
        alias_ = std::move(other.alias_);
        callId_ = std::move(other.callId_);
        signals_ = std::move(other.signals_);
    };

    std::string accountId_ {};
    std::string userName_ {};
    std::string alias_ {};
    uint16_t listeningPort_ {0};
    std::string toUri_ {};
    std::string callId_ {};
    std::vector<Signal> signals_;
    std::condition_variable cv_ {};
    std::mutex mtx_;
};

class PluginsTest : public CppUnit::TestFixture
{
public:
    PluginsTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~PluginsTest() { libjami::fini(); }
    static std::string name() { return "Plugins"; }
    void setUp();
    void tearDown();

    CallData aliceData;
    CallData bobData;

private:
    static bool waitForSignal(CallData& callData,
                              const std::string& signal,
                              const std::string& expectedEvent = {});
    // Event/Signal handlers
    static void onCallStateChange(const std::string& accountId,
                                  const std::string& callId,
                                  const std::string& state,
                                  CallData& callData);
    static void onIncomingCallWithMedia(const std::string& accountId,
                                        const std::string& callId,
                                        const std::vector<libjami::MediaMap> mediaList,
                                        CallData& callData);

    std::string name_{};
    std::string jplPath_{};
    std::string certPath_{};
    std::string pluginCertNotFound_{};
    std::string pluginNotSign_{};
    std::string pluginFileNotSign_{};
    std::string pluginManifestChanged_{};
    std::string pluginNotSignByIssuer_{};
    std::string pluginNotFoundPath_{};
    std::unique_ptr<dht::crypto::Certificate> cert_{};
    std::string installationPath_{};
    std::vector<std::string> mediaHandlers_{};
    std::vector<std::string> chatHandlers_{};

    void testEnable();
    void testCertificateVerification();
    void testSignatureVerification();
    void testInstallAndLoad();
    void testHandlers();
    void testDetailsAndPreferences();
    void testTranslations();
    void testCall();
    void testMessage();

    CPPUNIT_TEST_SUITE(PluginsTest);
    CPPUNIT_TEST(testEnable);
    CPPUNIT_TEST(testCertificateVerification);
    CPPUNIT_TEST(testSignatureVerification);
    CPPUNIT_TEST(testInstallAndLoad);
    CPPUNIT_TEST(testHandlers);
    CPPUNIT_TEST(testDetailsAndPreferences);
    CPPUNIT_TEST(testTranslations);
    CPPUNIT_TEST(testCall);
    CPPUNIT_TEST(testMessage);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(PluginsTest, PluginsTest::name());

void
PluginsTest::onIncomingCallWithMedia(const std::string& accountId,
                                        const std::string& callId,
                                        const std::vector<libjami::MediaMap> mediaList,
                                        CallData& callData)
{
    CPPUNIT_ASSERT_EQUAL(callData.accountId_, accountId);

    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - media count [%lu]",
              libjami::CallSignal::IncomingCallWithMedia::name,
              callData.alias_.c_str(),
              callId.c_str(),
              mediaList.size());

    // NOTE.
    // We shouldn't access shared_ptr<Call> as this event is supposed to mimic
    // the client, and the client have no access to this type. But here, we only
    // needed to check if the call exists. This is the most straightforward and
    // reliable way to do it until we add a new API (like hasCall(id)).
    if (not Manager::instance().getCallFromCallID(callId)) {
        JAMI_WARN("Call with ID [%s] does not exist!", callId.c_str());
        callData.callId_ = {};
        return;
    }

    std::unique_lock<std::mutex> lock {callData.mtx_};
    callData.callId_ = callId;
    callData.signals_.emplace_back(CallData::Signal(libjami::CallSignal::IncomingCallWithMedia::name));

    callData.cv_.notify_one();
}

void
PluginsTest::onCallStateChange(const std::string& accountId,
                                const std::string& callId,
                                const std::string& state,
                                CallData& callData)
{
    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - state [%s]",
              libjami::CallSignal::StateChange::name,
              callData.alias_.c_str(),
              callId.c_str(),
              state.c_str());

    CPPUNIT_ASSERT(accountId == callData.accountId_);

    {
        std::unique_lock<std::mutex> lock {callData.mtx_};
        callData.signals_.emplace_back(
            CallData::Signal(libjami::CallSignal::StateChange::name, state));
    }
    // NOTE. Only states that we are interested in will notify the CV.
    // If this unit test is modified to process other states, they must
    // be added here.
    if (state == "CURRENT" or state == "OVER" or state == "HUNGUP" or state == "RINGING") {
        callData.cv_.notify_one();
    }
}

void
PluginsTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob-no-upnp.yml");

    aliceData.accountId_ = actors["alice"];
    bobData.accountId_ = actors["bob"];

    // Configure Alice
    {
        CPPUNIT_ASSERT(not aliceData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<Account>(
            aliceData.accountId_);
        aliceData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        aliceData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
    }

    // Configure Bob
    {
        CPPUNIT_ASSERT(not bobData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<Account>(
            bobData.accountId_);
        bobData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        bobData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
    }

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> signalHandlers;

    // Insert needed signal handlers.
    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<libjami::MediaMap> mediaList) {
            if (aliceData.accountId_ == accountId)
                onIncomingCallWithMedia(accountId, callId, mediaList, aliceData);
            else if (bobData.accountId_ == accountId)
                onIncomingCallWithMedia(accountId, callId, mediaList, bobData);
        }));

    signalHandlers.insert(
        libjami::exportable_callback<libjami::CallSignal::StateChange>([&](const std::string& accountId,
                                                                       const std::string& callId,
                                                                       const std::string& state,
                                                                       signed) {
            if (aliceData.accountId_ == accountId)
                onCallStateChange(accountId, callId, state, aliceData);
            else if (bobData.accountId_ == accountId)
                onCallStateChange(accountId, callId, state, bobData);
        }));

    libjami::registerSignalHandlers(signalHandlers);
    std::ifstream file = jami::fileutils::ifstream("plugins/plugin.yml");
    assert(file.is_open());
    YAML::Node node = YAML::Load(file);

    assert(node.IsMap());

    name_ = node["plugin"].as<std::string>();
    certPath_ = node["cert"].as<std::string>();
    cert_ = std::make_unique<dht::crypto::Certificate>(fileutils::loadFile(node["jplDirectory"].as<std::string>() + DIR_SEPARATOR_CH + certPath_));
    dht::crypto::TrustList trust;
    trust.add(*cert_);
    jplPath_ = node["jplDirectory"].as<std::string>() + DIR_SEPARATOR_CH + name_ + ".jpl";
    installationPath_ = fileutils::get_data_dir() / "plugins" / name_;
    mediaHandlers_ = node["mediaHandlers"].as<std::vector<std::string>>();
    chatHandlers_ = node["chatHandlers"].as<std::vector<std::string>>();
    pluginNotFoundPath_ = node["jplDirectory"].as<std::string>() + DIR_SEPARATOR_CH + "fakePlugin.jpl";
    pluginCertNotFound_ = node["jplDirectory"].as<std::string>() + DIR_SEPARATOR_CH + node["pluginCertNotFound"].as<std::string>() + ".jpl";
    pluginNotSign_ = node["jplDirectory"].as<std::string>() + DIR_SEPARATOR_CH + node["pluginNotSign"].as<std::string>() + ".jpl";
    pluginFileNotSign_ = node["jplDirectory"].as<std::string>() + DIR_SEPARATOR_CH + node["pluginFileNotSign"].as<std::string>() + ".jpl";
    pluginManifestChanged_ = node["jplDirectory"].as<std::string>() + DIR_SEPARATOR_CH + node["pluginManifestChanged"].as<std::string>() + ".jpl";
    pluginNotSignByIssuer_ = node["jplDirectory"].as<std::string>() + DIR_SEPARATOR_CH + node["pluginNotSignByIssuer"].as<std::string>() + ".jpl";
}

void
PluginsTest::tearDown()
{
    libjami::unregisterSignalHandlers();
    wait_for_removal_of({aliceData.accountId_, bobData.accountId_});
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
PluginsTest::testSignatureVerification()
{
    // Test valid case
    Manager::instance().pluginPreferences.setPluginsEnabled(true);
    CPPUNIT_ASSERT(Manager::instance().getJamiPluginManager().checkPluginSignatureValidity(jplPath_, cert_.get()));
    CPPUNIT_ASSERT(Manager::instance().getJamiPluginManager().checkPluginSignatureFile(jplPath_));
    CPPUNIT_ASSERT(Manager::instance().getJamiPluginManager().checkPluginSignature(jplPath_, cert_.get()));

    std::string pluginNotFoundPath = "fakePlugin.jpl";
    // Test with a plugin that does not exist
    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().checkPluginSignatureFile(pluginNotFoundPath_));
    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().checkPluginSignature(pluginNotFoundPath_, cert_.get()));
    // Test with a plugin that does not have a signature
    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().checkPluginSignatureFile(pluginNotSign_));
    // Test with a plugin that does not have a file not signed
    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().checkPluginSignatureFile(pluginFileNotSign_));
    auto notCertSign = std::make_unique<dht::crypto::Certificate>();
    // Test with wrong certificate
    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().checkPluginSignatureValidity(jplPath_, notCertSign.get()));
    // Test with wrong signature
    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().checkPluginSignatureValidity(pluginManifestChanged_, cert_.get()));

}

void
PluginsTest::testCertificateVerification()
{

    std::string pluginNotFoundPath = "fakePlugin.jpl";
    Manager::instance().pluginPreferences.setPluginsEnabled(true);
    auto pluginCert = PluginUtils::readPluginCertificateFromArchive(jplPath_);
    Manager::instance().getJamiPluginManager().addPluginAuthority(*pluginCert);
    CPPUNIT_ASSERT(Manager::instance().getJamiPluginManager().checkPluginCertificate(jplPath_, true)->toString() == pluginCert->toString());
    CPPUNIT_ASSERT(Manager::instance().getJamiPluginManager().checkPluginCertificate(jplPath_, false)->toString() == pluginCert->toString());
    // create a plugin with not the same certificate

    auto pluginCertNotSignByIssuer = PluginUtils::readPluginCertificateFromArchive(pluginNotSignByIssuer_);
    CPPUNIT_ASSERT(Manager::instance().getJamiPluginManager().checkPluginCertificate(pluginNotSignByIssuer_, true)->toString() == pluginCertNotSignByIssuer->toString());
    // Test with a plugin that does not exist
    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().checkPluginCertificate(pluginNotFoundPath, false));
    // Test with a plugin that does not have a certificate
    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().checkPluginCertificate(pluginCertNotFound_, false));
    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().checkPluginCertificate(pluginNotSignByIssuer_, false));
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

    auto handlerLoaded = mediaHandlers_.size() + chatHandlers_.size(); // number of handlers expected
    for (auto handler : mediaHandlers)
    {
        auto details = Manager::instance().getJamiPluginManager().getCallServicesManager().getCallMediaHandlerDetails(handler);
        // check details expected for the test plugin
        if(std::find(mediaHandlers_.begin(),
                        mediaHandlers_.end(),
                        details["name"])
                   != mediaHandlers_.end()) {
            handlerLoaded--;
        }
    }
    for (auto handler : chatHandlers)
    {
        auto details = Manager::instance().getJamiPluginManager().getChatServicesManager().getChatHandlerDetails(handler);
        // check details expected for the test plugin
        if(std::find(chatHandlers_.begin(),
                        chatHandlers_.end(),
                        details["name"])
                   != chatHandlers_.end()) {
            handlerLoaded--;
        }
    }

    CPPUNIT_ASSERT(!handlerLoaded); // All expected handlers were found
    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().uninstallPlugin(installationPath_));
}

void
PluginsTest::testDetailsAndPreferences()
{
    Manager::instance().pluginPreferences.setPluginsEnabled(true);
    Manager::instance().getJamiPluginManager().installPlugin(jplPath_, true);
    // Unload now to avoid reloads when changing the preferences
    Manager::instance().getJamiPluginManager().unloadPlugin(installationPath_);

    // Details
    auto details = Manager::instance().getJamiPluginManager().getPluginDetails(installationPath_);
    CPPUNIT_ASSERT(details["name"] == name_);

    // Get-set-reset - no account
    auto preferences = Manager::instance().getJamiPluginManager().getPluginPreferences(installationPath_, "");
    CPPUNIT_ASSERT(!preferences.empty());
    auto preferencesValuesOrig = Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, "");

    std::string preferenceNewValue = aliceData.accountId_;
    auto key = preferences[0]["key"];
    CPPUNIT_ASSERT(Manager::instance().getJamiPluginManager().setPluginPreference(installationPath_, "", key, preferenceNewValue));

    // Test global preference change
    auto preferencesValuesNew = Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, "");
    CPPUNIT_ASSERT(preferencesValuesOrig[key] != preferencesValuesNew[key]);
    CPPUNIT_ASSERT(preferencesValuesNew[key] == preferenceNewValue);

    // Test global preference change in an account
    preferencesValuesNew = Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, aliceData.accountId_);
    CPPUNIT_ASSERT(preferencesValuesOrig[key] != preferencesValuesNew[key]);
    CPPUNIT_ASSERT(preferencesValuesNew[key] == preferenceNewValue);

    // Test reset global preference change
    Manager::instance().getJamiPluginManager().resetPluginPreferencesValuesMap(installationPath_, "");
    preferencesValuesNew = Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, "");
    CPPUNIT_ASSERT(preferencesValuesOrig[key] == preferencesValuesNew[key]);
    CPPUNIT_ASSERT(preferencesValuesNew[key] != preferenceNewValue);

    // Get-set-reset - alice account
    preferences = Manager::instance().getJamiPluginManager().getPluginPreferences(installationPath_, aliceData.accountId_);
    CPPUNIT_ASSERT(!preferences.empty());
    preferencesValuesOrig = Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, aliceData.accountId_);
    auto preferencesValuesBobOrig = Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, bobData.accountId_);

    key = preferences[0]["key"];
    CPPUNIT_ASSERT(Manager::instance().getJamiPluginManager().setPluginPreference(installationPath_, aliceData.accountId_, key, preferenceNewValue));

    // Test account preference change
    preferencesValuesNew = Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, aliceData.accountId_);
    auto preferencesValuesBobNew = Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, bobData.accountId_);
    CPPUNIT_ASSERT(preferencesValuesBobNew[key] == preferencesValuesBobOrig[key]);
    CPPUNIT_ASSERT(preferencesValuesOrig[key] != preferencesValuesNew[key]);
    CPPUNIT_ASSERT(preferencesValuesOrig[key] == preferencesValuesBobOrig[key]);
    CPPUNIT_ASSERT(preferencesValuesNew[key] != preferencesValuesBobOrig[key]);
    CPPUNIT_ASSERT(preferencesValuesNew[key] == preferenceNewValue);

    // Test account preference change with global preference reset
    Manager::instance().getJamiPluginManager().resetPluginPreferencesValuesMap(installationPath_, "");
    preferencesValuesNew = Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, aliceData.accountId_);
    preferencesValuesBobNew = Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, bobData.accountId_);
    CPPUNIT_ASSERT(preferencesValuesBobNew[key] == preferencesValuesBobOrig[key]);
    CPPUNIT_ASSERT(preferencesValuesOrig[key] != preferencesValuesNew[key]);
    CPPUNIT_ASSERT(preferencesValuesOrig[key] == preferencesValuesBobOrig[key]);
    CPPUNIT_ASSERT(preferencesValuesNew[key] != preferencesValuesBobOrig[key]);
    CPPUNIT_ASSERT(preferencesValuesNew[key] == preferenceNewValue);

    // Test account preference reset
    Manager::instance().getJamiPluginManager().resetPluginPreferencesValuesMap(installationPath_, aliceData.accountId_);
    preferencesValuesNew = Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, aliceData.accountId_);
    preferencesValuesBobNew = Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, bobData.accountId_);
    CPPUNIT_ASSERT(preferencesValuesBobNew[key] == preferencesValuesBobOrig[key]);
    CPPUNIT_ASSERT(preferencesValuesOrig[key] == preferencesValuesNew[key]);
    CPPUNIT_ASSERT(preferencesValuesOrig[key] == preferencesValuesBobOrig[key]);
    CPPUNIT_ASSERT(preferencesValuesNew[key] == preferencesValuesBobOrig[key]);
    CPPUNIT_ASSERT(preferencesValuesNew[key] != preferenceNewValue);

    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().uninstallPlugin(installationPath_));
}

void
PluginsTest::testTranslations()
{
    Manager::instance().pluginPreferences.setPluginsEnabled(true);
    setenv("JAMI_LANG", "en", true);
    Manager::instance().getJamiPluginManager().installPlugin(jplPath_, true);

    auto preferencesValuesEN = Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, "");
    auto detailsValuesEN = Manager::instance().getJamiPluginManager().getPluginDetails(installationPath_, true);

    CPPUNIT_ASSERT(!preferencesValuesEN.empty());
    CPPUNIT_ASSERT(!detailsValuesEN.empty());

    setenv("JAMI_LANG", "fr", true);

    CPPUNIT_ASSERT(Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, "") != preferencesValuesEN);
    CPPUNIT_ASSERT(Manager::instance().getJamiPluginManager().getPluginDetails(installationPath_, true) != detailsValuesEN);

    setenv("JAMI_LANG", "en", true);

    CPPUNIT_ASSERT(Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(installationPath_, "") == preferencesValuesEN);
    CPPUNIT_ASSERT(Manager::instance().getJamiPluginManager().getPluginDetails(installationPath_, true) == detailsValuesEN);

    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().uninstallPlugin(installationPath_));
}

bool
PluginsTest::waitForSignal(CallData& callData,
                            const std::string& expectedSignal,
                            const std::string& expectedEvent)
{
    const std::chrono::seconds TIME_OUT {30};
    std::unique_lock<std::mutex> lock {callData.mtx_};

    // Combined signal + event (if any).
    std::string sigEvent(expectedSignal);
    if (not expectedEvent.empty())
        sigEvent += "::" + expectedEvent;

    JAMI_INFO("[%s] is waiting for [%s] signal/event", callData.alias_.c_str(), sigEvent.c_str());

    auto res = callData.cv_.wait_for(lock, TIME_OUT, [&] {
        // Search for the expected signal in list of received signals.
        bool pred = false;
        for (auto it = callData.signals_.begin(); it != callData.signals_.end(); it++) {
            // The predicate is true if the signal names match, and if the
            // expectedEvent is not empty, the events must also match.
            if (it->name_ == expectedSignal
                and (expectedEvent.empty() or it->event_ == expectedEvent)) {
                pred = true;
                // Done with this signal.
                callData.signals_.erase(it);
                break;
            }
        }

        return pred;
    });

    if (not res) {
        JAMI_ERR("[%s] waiting for signal/event [%s] timed-out!",
                 callData.alias_.c_str(),
                 sigEvent.c_str());

        JAMI_INFO("[%s] currently has the following signals:", callData.alias_.c_str());

        for (auto const& sig : callData.signals_) {
            JAMI_INFO() << "\tSignal [" << sig.name_
                        << (sig.event_.empty() ? "" : ("::" + sig.event_)) << "]";
        }
    }

    return res;
}

void
PluginsTest::testCall()
{
    Manager::instance().pluginPreferences.setPluginsEnabled(true);
    Manager::instance().getJamiPluginManager().installPlugin(jplPath_, true);

    // alice calls bob
    // for handler available, toggle - check status - untoggle - checkstatus
    // end call

    MediaAttribute defaultAudio(MediaType::MEDIA_AUDIO);
    defaultAudio.label_ = "audio_0";
    defaultAudio.enabled_ = true;

    MediaAttribute defaultVideo(MediaType::MEDIA_VIDEO);
    defaultVideo.label_ = "video_0";
    defaultVideo.enabled_ = true;

    std::vector<MediaAttribute> request;
    std::vector<MediaAttribute> answer;
    // First offer/answer
    request.emplace_back(MediaAttribute(defaultAudio));
    request.emplace_back(MediaAttribute(defaultVideo));
    answer.emplace_back(MediaAttribute(defaultAudio));
    answer.emplace_back(MediaAttribute(defaultVideo));

    JAMI_INFO("Start call between alice and Bob");
    aliceData.callId_ = libjami::placeCallWithMedia(aliceData.accountId_, bobData.userName_, MediaAttribute::mediaAttributesToMediaMaps(request));
    CPPUNIT_ASSERT(not aliceData.callId_.empty());

    auto aliceCall = std::static_pointer_cast<SIPCall>(
        Manager::instance().getCallFromCallID(aliceData.callId_));
    CPPUNIT_ASSERT(aliceCall);

    aliceData.callId_ = aliceCall->getCallId();

    JAMI_INFO("ALICE [%s] started a call with BOB [%s] and wait for answer",
              aliceData.accountId_.c_str(),
              bobData.accountId_.c_str());

    // Wait for incoming call signal.
    CPPUNIT_ASSERT(waitForSignal(bobData, libjami::CallSignal::IncomingCallWithMedia::name));

    // Answer the call.
    {
        libjami::acceptWithMedia(bobData.accountId_, bobData.callId_, MediaAttribute::mediaAttributesToMediaMaps(answer));
    }

    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(bobData,
                                       libjami::CallSignal::StateChange::name,
                                       libjami::Call::StateEvent::CURRENT));

    JAMI_INFO("BOB answered the call [%s]", bobData.callId_.c_str());

    std::this_thread::sleep_for(std::chrono::seconds(3));
    auto mediaHandlers = Manager::instance().getJamiPluginManager().getCallServicesManager().getCallMediaHandlers();

    for (auto handler : mediaHandlers)
    {
        auto details = Manager::instance().getJamiPluginManager().getCallServicesManager().getCallMediaHandlerDetails(handler);
        // check details expected for the test plugin
        if(std::find(mediaHandlers_.begin(),
                        mediaHandlers_.end(),
                        details["name"])
                   != mediaHandlers_.end()) {
            Manager::instance().getJamiPluginManager().getCallServicesManager().toggleCallMediaHandler(handler, aliceData.callId_, true);
            auto statusMap = Manager::instance().getJamiPluginManager().getCallServicesManager().getCallMediaHandlerStatus(aliceData.callId_);
            CPPUNIT_ASSERT(std::find(statusMap.begin(), statusMap.end(), handler) != statusMap.end());

            Manager::instance().getJamiPluginManager().getCallServicesManager().toggleCallMediaHandler(handler, aliceData.callId_, false);
            statusMap = Manager::instance().getJamiPluginManager().getCallServicesManager().getCallMediaHandlerStatus(aliceData.callId_);
            CPPUNIT_ASSERT(std::find(statusMap.begin(), statusMap.end(), handler) == statusMap.end());
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));
    // Bob hang-up.
    JAMI_INFO("Hang up BOB's call and wait for ALICE to hang up");
    libjami::hangUp(bobData.accountId_, bobData.callId_);

    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData,
                                       libjami::CallSignal::StateChange::name,
                                       libjami::Call::StateEvent::HUNGUP));

    JAMI_INFO("Call terminated on both sides");
    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().uninstallPlugin(installationPath_));
}

void
PluginsTest::testMessage()
{
    Manager::instance().pluginPreferences.setPluginsEnabled(true);
    Manager::instance().getJamiPluginManager().installPlugin(jplPath_, true);

    // alice and bob chat
    // for handler available, toggle - check status - untoggle - checkstatus
    // end call

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == bobData.accountId_) {
                messageBobReceived += 1;
            } else {
                messageAliceReceived += 1;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobData.accountId_) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    auto convId = libjami::startConversation(aliceData.accountId_);

    libjami::addConversationMember(aliceData.accountId_, convId, bobData.userName_);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    libjami::acceptConversationRequest(bobData.accountId_, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() / bobData.accountId_
                    / "conversations" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Wait that alice sees Bob
    cv.wait_for(lk, 30s, [&]() { return messageAliceReceived == 2; });

    auto chatHandlers = Manager::instance().getJamiPluginManager().getChatServicesManager().getChatHandlers();

    for (auto handler : chatHandlers)
    {
        auto details = Manager::instance().getJamiPluginManager().getChatServicesManager().getChatHandlerDetails(handler);
        // check details expected for the test plugin
        if(std::find(chatHandlers_.begin(),
                        chatHandlers_.end(),
                        details["name"])
                   != chatHandlers_.end()) {
            Manager::instance().getJamiPluginManager().getChatServicesManager().toggleChatHandler(handler, aliceData.accountId_, convId, true);
            auto statusMap = Manager::instance().getJamiPluginManager().getChatServicesManager().getChatHandlerStatus(aliceData.accountId_, convId);
            CPPUNIT_ASSERT(std::find(statusMap.begin(), statusMap.end(), handler) != statusMap.end());

            libjami::sendMessage(aliceData.accountId_, convId, "hi"s, "");
            cv.wait_for(lk, 30s, [&]() { return messageBobReceived == 1; });

            Manager::instance().getJamiPluginManager().getChatServicesManager().toggleChatHandler(handler, aliceData.accountId_, convId, false);
            statusMap = Manager::instance().getJamiPluginManager().getChatServicesManager().getChatHandlerStatus(aliceData.accountId_, convId);
            CPPUNIT_ASSERT(std::find(statusMap.begin(), statusMap.end(), handler) == statusMap.end());
        }
    }

    CPPUNIT_ASSERT(!Manager::instance().getJamiPluginManager().uninstallPlugin(installationPath_));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::PluginsTest::name())
