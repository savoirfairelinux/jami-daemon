/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include "sip/sipaccountbase.h"
#include "sip/sipvoiplink.h"

#ifdef ENABLE_VIDEO
#include "libav_utils.h"
#endif

#include "account_schema.h"
#include "manager.h"
#include "connectivity/ice_transport.h"

#include "config/yamlparser.h"

#include "client/ring_signal.h"
#include "jami/account_const.h"
#include "string_utils.h"
#include "fileutils.h"
#include "connectivity/sip_utils.h"
#include "connectivity/utf8_utils.h"
#include "uri.h"

#include "manager.h"
#ifdef ENABLE_PLUGIN
#include "plugin/jamipluginmanager.h"
#include "plugin/streamdata.h"
#endif

#include <fmt/core.h>
#include <json/json.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <yaml-cpp/yaml.h>
#pragma GCC diagnostic pop

#include <type_traits>
#include <regex>
#include <ctime>

using namespace std::literals;

namespace jami {

SIPAccountBase::SIPAccountBase(const std::string& accountID)
    : Account(accountID)
    , messageEngine_(*this,
                     fileutils::get_cache_dir() + DIR_SEPARATOR_STR + getAccountID()
                         + DIR_SEPARATOR_STR "messages")
    , link_(Manager::instance().sipVoIPLink())
{}

SIPAccountBase::~SIPAccountBase() noexcept {}

bool
SIPAccountBase::CreateClientDialogAndInvite(const pj_str_t* from,
                                            const pj_str_t* contact,
                                            const pj_str_t* to,
                                            const pj_str_t* target,
                                            const pjmedia_sdp_session* local_sdp,
                                            pjsip_dialog** dlg,
                                            pjsip_inv_session** inv)
{
    JAMI_DBG("Creating SIP dialog: \n"
             "From: %s\n"
             "Contact: %s\n"
             "To: %s\n",
             from->ptr,
             contact->ptr,
             to->ptr);

    if (target) {
        JAMI_DBG("Target: %s", target->ptr);
    } else {
        JAMI_DBG("No target provided, using 'to' as target");
    }

    if (pjsip_dlg_create_uac(pjsip_ua_instance(), from, contact, to, target, dlg) != PJ_SUCCESS) {
        JAMI_ERR("Unable to create SIP dialogs for user agent client when calling %s", to->ptr);
        return false;
    }

    auto dialog = *dlg;

    {
        // lock dialog until invite session creation; this one will own the dialog after
        sip_utils::PJDialogLock dlg_lock {dialog};

        // Append "Subject: Phone Call" header
        constexpr auto subj_hdr_name = sip_utils::CONST_PJ_STR("Subject");
        auto subj_hdr = reinterpret_cast<pjsip_hdr*>(
            pjsip_parse_hdr(dialog->pool,
                            &subj_hdr_name,
                            const_cast<char*>("Phone call"),
                            10,
                            nullptr));
        pj_list_push_back(&dialog->inv_hdr, subj_hdr);

        if (pjsip_inv_create_uac(dialog, local_sdp, 0, inv) != PJ_SUCCESS) {
            JAMI_ERR("Unable to create invite session for user agent client");
            return false;
        }
    }

    return true;
}

void
SIPAccountBase::flush()
{
    // Class base method
    Account::flush();

    fileutils::remove(fileutils::get_cache_dir() + DIR_SEPARATOR_STR + getAccountID()
                      + DIR_SEPARATOR_STR "messages");
}

template<typename T>
static void
validate(std::string& member, const std::string& param, const T& valid)
{
    const auto begin = std::begin(valid);
    const auto end = std::end(valid);
    if (find(begin, end, param) != end)
        member = param;
    else
        JAMI_ERR("Invalid parameter \"%s\"", param.c_str());
}

static void
updateRange(uint16_t min, uint16_t max, std::pair<uint16_t, uint16_t>& range)
{
    if (min > 0 and (max > min) and max <= SIPAccountBase::MAX_PORT - 2) {
        range.first = min;
        range.second = max;
    }
}

static void
unserializeRange(const YAML::Node& node,
                 const char* minKey,
                 const char* maxKey,
                 std::pair<uint16_t, uint16_t>& range)
{
    int tmpMin = 0;
    int tmpMax = 0;
    yaml_utils::parseValue(node, minKey, tmpMin);
    yaml_utils::parseValue(node, maxKey, tmpMax);
    updateRange(tmpMin, tmpMax, range);
}

static void
addRangeToDetails(std::map<std::string, std::string>& a,
                  const char* minKey,
                  const char* maxKey,
                  const std::pair<uint16_t, uint16_t>& range)
{
    a.emplace(minKey, std::to_string(range.first));
    a.emplace(maxKey, std::to_string(range.second));
}

void
SIPAccountBase::serialize(YAML::Emitter& out) const
{
    Account::serialize(out);

    out << YAML::Key << Conf::AUDIO_PORT_MAX_KEY << YAML::Value << audioPortRange_.second;
    out << YAML::Key << Conf::AUDIO_PORT_MIN_KEY << YAML::Value << audioPortRange_.first;
    out << YAML::Key << Conf::DTMF_TYPE_KEY << YAML::Value << dtmfType_;
    out << YAML::Key << Conf::INTERFACE_KEY << YAML::Value << interface_;
    out << YAML::Key << Conf::SAME_AS_LOCAL_KEY << YAML::Value << publishedSameasLocal_;

    out << YAML::Key << VIDEO_ENABLED_KEY << YAML::Value << videoEnabled_;
    out << YAML::Key << Conf::VIDEO_PORT_MAX_KEY << YAML::Value << videoPortRange_.second;
    out << YAML::Key << Conf::VIDEO_PORT_MIN_KEY << YAML::Value << videoPortRange_.first;
}

void
SIPAccountBase::unserialize(const YAML::Node& node)
{
    using yaml_utils::parseValue;
    using yaml_utils::parseValueOptional;
    using yaml_utils::parseVectorMap;

    Account::unserialize(node);

    parseValue(node, VIDEO_ENABLED_KEY, videoEnabled_);

    parseValue(node, Conf::INTERFACE_KEY, interface_);
    parseValue(node, Conf::SAME_AS_LOCAL_KEY, publishedSameasLocal_);

    parseValue(node, Conf::DTMF_TYPE_KEY, dtmfType_);

    unserializeRange(node, Conf::AUDIO_PORT_MIN_KEY, Conf::AUDIO_PORT_MAX_KEY, audioPortRange_);
    unserializeRange(node, Conf::VIDEO_PORT_MIN_KEY, Conf::VIDEO_PORT_MAX_KEY, videoPortRange_);
}

void
SIPAccountBase::setAccountDetails(const std::map<std::string, std::string>& details)
{
    Account::setAccountDetails(details);

    parseBool(details, Conf::CONFIG_VIDEO_ENABLED, videoEnabled_);

    // general sip settings
    parseString(details, Conf::CONFIG_LOCAL_INTERFACE, interface_);
    parseBool(details, Conf::CONFIG_PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal_);

    parseString(details, Conf::CONFIG_ACCOUNT_DTMF_TYPE, dtmfType_);

    int tmpMin = -1;
    parseInt(details, Conf::CONFIG_ACCOUNT_AUDIO_PORT_MIN, tmpMin);
    int tmpMax = -1;
    parseInt(details, Conf::CONFIG_ACCOUNT_AUDIO_PORT_MAX, tmpMax);
    updateRange(tmpMin, tmpMax, audioPortRange_);
#ifdef ENABLE_VIDEO
    tmpMin = -1;
    parseInt(details, Conf::CONFIG_ACCOUNT_VIDEO_PORT_MIN, tmpMin);
    tmpMax = -1;
    parseInt(details, Conf::CONFIG_ACCOUNT_VIDEO_PORT_MAX, tmpMax);
    updateRange(tmpMin, tmpMax, videoPortRange_);
#endif
}

std::map<std::string, std::string>
SIPAccountBase::getAccountDetails() const
{
    auto a = Account::getAccountDetails();
    a.emplace(Conf::CONFIG_VIDEO_ENABLED, videoEnabled_ ? TRUE_STR : FALSE_STR);

    addRangeToDetails(a,
                      Conf::CONFIG_ACCOUNT_AUDIO_PORT_MIN,
                      Conf::CONFIG_ACCOUNT_AUDIO_PORT_MAX,
                      audioPortRange_);
#ifdef ENABLE_VIDEO
    addRangeToDetails(a,
                      Conf::CONFIG_ACCOUNT_VIDEO_PORT_MIN,
                      Conf::CONFIG_ACCOUNT_VIDEO_PORT_MAX,
                      videoPortRange_);
#endif

    a.emplace(Conf::CONFIG_ACCOUNT_DTMF_TYPE, dtmfType_);
    a.emplace(Conf::CONFIG_LOCAL_INTERFACE, interface_);

    return a;
}

std::map<std::string, std::string>
SIPAccountBase::getVolatileAccountDetails() const
{
    auto a = Account::getVolatileAccountDetails();

    // replace value from Account for IP2IP
    if (isIP2IP())
        a[Conf::CONFIG_ACCOUNT_REGISTRATION_STATUS] = "READY";

    a.emplace(Conf::CONFIG_TRANSPORT_STATE_CODE, std::to_string(transportStatus_));
    a.emplace(Conf::CONFIG_TRANSPORT_STATE_DESC, transportError_);
    return a;
}

void
SIPAccountBase::setRegistrationState(RegistrationState state,
                                     unsigned details_code,
                                     const std::string& details_str)
{
    if (state == RegistrationState::REGISTERED
        && registrationState_ != RegistrationState::REGISTERED)
        messageEngine_.load();
    else if (state != RegistrationState::REGISTERED
             && registrationState_ == RegistrationState::REGISTERED)
        messageEngine_.save();
    Account::setRegistrationState(state, details_code, details_str);
}

auto
SIPAccountBase::getPortsReservation() noexcept -> decltype(getPortsReservation())
{
    // Note: static arrays are zero-initialized
    static std::remove_reference<decltype(getPortsReservation())>::type portsInUse;
    return portsInUse;
}

uint16_t
SIPAccountBase::getRandomEvenPort(const std::pair<uint16_t, uint16_t>& range) const
{
    std::uniform_int_distribution<uint16_t> dist(range.first / 2, range.second / 2);
    uint16_t result;
    do {
        result = 2 * dist(rand);
    } while (getPortsReservation()[result / 2]);
    return result;
}

uint16_t
SIPAccountBase::acquireRandomEvenPort(const std::pair<uint16_t, uint16_t>& range) const
{
    std::uniform_int_distribution<uint16_t> dist(range.first / 2, range.second / 2);
    uint16_t result;

    do {
        result = 2 * dist(rand);
    } while (getPortsReservation()[result / 2]);

    getPortsReservation()[result / 2] = true;
    return result;
}

uint16_t
SIPAccountBase::acquirePort(uint16_t port)
{
    getPortsReservation()[port / 2] = true;
    return port;
}

void
SIPAccountBase::releasePort(uint16_t port) noexcept
{
    getPortsReservation()[port / 2] = false;
}

uint16_t
SIPAccountBase::generateAudioPort() const
{
    return acquireRandomEvenPort(audioPortRange_);
}

#ifdef ENABLE_VIDEO
uint16_t
SIPAccountBase::generateVideoPort() const
{
    return acquireRandomEvenPort(videoPortRange_);
}
#endif

void
SIPAccountBase::onTextMessage(const std::string& id,
                              const std::string& from,
                              const std::string& /* deviceId */,
                              const std::map<std::string, std::string>& payloads)
{
    JAMI_DBG("Text message received from %s, %zu part(s)", from.c_str(), payloads.size());
    for (const auto& m : payloads) {
        if (!utf8_validate(m.first))
            return;
        if (!utf8_validate(m.second)) {
            JAMI_WARN("Dropping invalid message with MIME type %s", m.first.c_str());
            return;
        }
        if (handleMessage(from, m))
            return;
    }

#ifdef ENABLE_PLUGIN
    auto& pluginChatManager = Manager::instance().getJamiPluginManager().getChatServicesManager();
    if (pluginChatManager.hasHandlers()) {
        pluginChatManager.publishMessage(
            std::make_shared<JamiMessage>(accountID_, from, true, payloads, false));
    }
#endif
    emitSignal<libjami::ConfigurationSignal::IncomingAccountMessage>(accountID_, from, id, payloads);

    libjami::Message message;
    message.from = from;
    message.payloads = payloads;
    message.received = std::time(nullptr);
    std::lock_guard<std::mutex> lck(mutexLastMessages_);
    lastMessages_.emplace_back(std::move(message));
    while (lastMessages_.size() > MAX_WAITING_MESSAGES_SIZE) {
        lastMessages_.pop_front();
    }
}

std::vector<libjami::Message>
SIPAccountBase::getLastMessages(const uint64_t& base_timestamp)
{
    std::lock_guard<std::mutex> lck(mutexLastMessages_);
    auto it = lastMessages_.begin();
    size_t num = lastMessages_.size();
    while (it != lastMessages_.end() and it->received <= base_timestamp) {
        num--;
        ++it;
    }
    if (num == 0)
        return {};
    return {it, lastMessages_.end()};
}

std::vector<MediaAttribute>
SIPAccountBase::createDefaultMediaList(bool addVideo, bool onHold)
{
    std::vector<MediaAttribute> mediaList;
    bool secure = isSrtpEnabled();
    // Add audio and DTMF events
    mediaList.emplace_back(MediaAttribute(MediaType::MEDIA_AUDIO,
                                          false,
                                          secure,
                                          true,
                                          "",
                                          sip_utils::DEFAULT_AUDIO_STREAMID,
                                          onHold));

#ifdef ENABLE_VIDEO
    // Add video if allowed.
    if (isVideoEnabled() and addVideo) {
        mediaList.emplace_back(MediaAttribute(MediaType::MEDIA_VIDEO,
                                              false,
                                              secure,
                                              true,
                                              "",
                                              sip_utils::DEFAULT_VIDEO_STREAMID,
                                              onHold));
    }
#endif
    return mediaList;
}
} // namespace jami
