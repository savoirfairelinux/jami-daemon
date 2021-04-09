/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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

#include "sipaccountbase.h"
#include "sipvoiplink.h"

#ifdef ENABLE_VIDEO
#include "libav_utils.h"
#endif

#include "account_schema.h"
#include "manager.h"
#include "ice_transport.h"

#include "config/yamlparser.h"

#include "client/ring_signal.h"
#include "dring/account_const.h"
#include "string_utils.h"
#include "fileutils.h"
#include "sip_utils.h"
#include "utf8_utils.h"
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

static constexpr const char MIME_TYPE_IMDN[] {"message/imdn+xml"};
static constexpr const char MIME_TYPE_GIT[] {"application/im-gitmessage-id"};
static constexpr const char MIME_TYPE_INVITE_JSON[] {"application/invite+json"};
static constexpr const char MIME_TYPE_INVITE[] {"application/invite"};
static constexpr const char MIME_TYPE_IM_COMPOSING[] {"application/im-iscomposing+xml"};
static constexpr std::chrono::steady_clock::duration COMPOSING_TIMEOUT {std::chrono::seconds(12)};

SIPAccountBase::SIPAccountBase(const std::string& accountID)
    : Account(accountID)
    , messageEngine_(*this,
                     fileutils::get_cache_dir() + DIR_SEPARATOR_STR + getAccountID()
                         + DIR_SEPARATOR_STR "messages")
    , link_(Manager::instance().sipVoIPLink())
{}

SIPAccountBase::~SIPAccountBase() {}

bool
SIPAccountBase::CreateClientDialogAndInvite(const pj_str_t* from,
                                            const pj_str_t* contact,
                                            const pj_str_t* to,
                                            const pj_str_t* target,
                                            const pjmedia_sdp_session* local_sdp,
                                            pjsip_dialog** dlg,
                                            pjsip_inv_session** inv)
{
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

std::string
getIsComposing(const std::string& conversationId, bool isWriting)
{
    // implementing https://tools.ietf.org/rfc/rfc3994.txt
    return fmt::format("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
                       "<isComposing><state>{}</state>{}</isComposing>",
                       isWriting ? "active"sv : "idle"sv,
                       conversationId.empty()? "" : "<conversation>" + conversationId + "</conversation>");
}

std::string
getDisplayed(const std::string& conversationId, const std::string& messageId)
{
    // implementing https://tools.ietf.org/rfc/rfc5438.txt
    return fmt::format("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
                       "<imdn><message-id>{}</message-id>\n"
                       "{}"
                       "<display-notification><status><displayed/></status></display-notification>\n"
                       "</imdn>",
                       messageId,
                       conversationId.empty()? "" : "<conversation>" + conversationId + "</conversation>");
}

void
SIPAccountBase::setIsComposing(const std::string& conversationUri, bool isWriting)
{
    Uri uri(conversationUri);
    std::string conversationId = {};
    if (uri.scheme() == Uri::Scheme::SWARM)
        conversationId = uri.authority();
    auto uid = uri.authority();

    if (not isWriting and conversationUri != composingUri_)
        return;

    if (composingTimeout_) {
        composingTimeout_->cancel();
        composingTimeout_.reset();
    }
    if (isWriting) {
        if (not composingUri_.empty() and composingUri_ != conversationUri) {
            sendInstantMessage(uid,
                               {{MIME_TYPE_IM_COMPOSING, getIsComposing(conversationId, false)}});
            composingTime_ = std::chrono::steady_clock::time_point::min();
        }
        composingUri_.clear();
        composingUri_.insert(composingUri_.end(), conversationUri.begin(), conversationUri.end());
        auto now = std::chrono::steady_clock::now();
        if (now >= composingTime_ + COMPOSING_TIMEOUT) {
            sendInstantMessage(uid,
                               {{MIME_TYPE_IM_COMPOSING, getIsComposing(conversationId, true)}});
            composingTime_ = now;
        }
        std::weak_ptr<SIPAccountBase> weak = std::static_pointer_cast<SIPAccountBase>(
            shared_from_this());
        composingTimeout_ = Manager::instance().scheduleTask(
            [weak, uid, conversationId]() {
                if (auto sthis = weak.lock()) {
                    sthis->sendInstantMessage(uid,
                                              {{MIME_TYPE_IM_COMPOSING,
                                                getIsComposing(conversationId, false)}});
                    sthis->composingUri_.clear();
                    sthis->composingTime_ = std::chrono::steady_clock::time_point::min();
                }
            },
            now + COMPOSING_TIMEOUT);
    } else {
        sendInstantMessage(uid, {{MIME_TYPE_IM_COMPOSING, getIsComposing(conversationId, false)}});
        composingUri_.clear();
        composingTime_ = std::chrono::steady_clock::time_point::min();
    }
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
    out << YAML::Key << Conf::PUBLISH_ADDR_KEY << YAML::Value << publishedIpAddress_;
    out << YAML::Key << Conf::PUBLISH_PORT_KEY << YAML::Value << publishedPort_;
    out << YAML::Key << Conf::SAME_AS_LOCAL_KEY << YAML::Value << publishedSameasLocal_;

    out << YAML::Key << VIDEO_ENABLED_KEY << YAML::Value << videoEnabled_;
    out << YAML::Key << Conf::VIDEO_PORT_MAX_KEY << YAML::Value << videoPortRange_.second;
    out << YAML::Key << Conf::VIDEO_PORT_MIN_KEY << YAML::Value << videoPortRange_.first;

    out << YAML::Key << Conf::STUN_ENABLED_KEY << YAML::Value << stunEnabled_;
    out << YAML::Key << Conf::STUN_SERVER_KEY << YAML::Value << stunServer_;
    out << YAML::Key << Conf::TURN_ENABLED_KEY << YAML::Value << turnEnabled_;
    out << YAML::Key << Conf::TURN_SERVER_KEY << YAML::Value << turnServer_;
    out << YAML::Key << Conf::TURN_SERVER_UNAME_KEY << YAML::Value << turnServerUserName_;
    out << YAML::Key << Conf::TURN_SERVER_PWD_KEY << YAML::Value << turnServerPwd_;
    out << YAML::Key << Conf::TURN_SERVER_REALM_KEY << YAML::Value << turnServerRealm_;
}

void
SIPAccountBase::serializeTls(YAML::Emitter& out) const
{
    out << YAML::Key << Conf::CALIST_KEY << YAML::Value << tlsCaListFile_;
    out << YAML::Key << Conf::CERTIFICATE_KEY << YAML::Value << tlsCertificateFile_;
    out << YAML::Key << Conf::TLS_PASSWORD_KEY << YAML::Value << tlsPassword_;
    out << YAML::Key << Conf::PRIVATE_KEY_KEY << YAML::Value << tlsPrivateKeyFile_;
}

void
SIPAccountBase::unserialize(const YAML::Node& node)
{
    using yaml_utils::parseValue;
    using yaml_utils::parseVectorMap;

    Account::unserialize(node);

    parseValue(node, VIDEO_ENABLED_KEY, videoEnabled_);

    parseValue(node, Conf::INTERFACE_KEY, interface_);
    parseValue(node, Conf::SAME_AS_LOCAL_KEY, publishedSameasLocal_);
    std::string publishedIpAddress;
    parseValue(node, Conf::PUBLISH_ADDR_KEY, publishedIpAddress);
    IpAddr publishedIp {publishedIpAddress};
    if (publishedIp and not publishedSameasLocal_)
        setPublishedAddress(publishedIp);

    int port = sip_utils::DEFAULT_SIP_PORT;
    parseValue(node, Conf::PUBLISH_PORT_KEY, port);
    publishedPort_ = port;

    parseValue(node, Conf::DTMF_TYPE_KEY, dtmfType_);

    unserializeRange(node, Conf::AUDIO_PORT_MIN_KEY, Conf::AUDIO_PORT_MAX_KEY, audioPortRange_);
    unserializeRange(node, Conf::VIDEO_PORT_MIN_KEY, Conf::VIDEO_PORT_MAX_KEY, videoPortRange_);

    // ICE - STUN/TURN
    if (not isIP2IP()) {
        parseValue(node, Conf::STUN_ENABLED_KEY, stunEnabled_);
        parseValue(node, Conf::STUN_SERVER_KEY, stunServer_);
        parseValue(node, Conf::TURN_ENABLED_KEY, turnEnabled_);
        parseValue(node, Conf::TURN_SERVER_KEY, turnServer_);
        parseValue(node, Conf::TURN_SERVER_UNAME_KEY, turnServerUserName_);
        parseValue(node, Conf::TURN_SERVER_PWD_KEY, turnServerPwd_);
        parseValue(node, Conf::TURN_SERVER_REALM_KEY, turnServerRealm_);
    }
}

void
SIPAccountBase::setAccountDetails(const std::map<std::string, std::string>& details)
{
    Account::setAccountDetails(details);

    parseBool(details, Conf::CONFIG_VIDEO_ENABLED, videoEnabled_);

    // general sip settings
    parseString(details, Conf::CONFIG_LOCAL_INTERFACE, interface_);
    parseBool(details, Conf::CONFIG_PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal_);
    parseString(details, Conf::CONFIG_PUBLISHED_ADDRESS, publishedIpAddress_);
    parseInt(details, Conf::CONFIG_PUBLISHED_PORT, publishedPort_);

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

    // ICE - STUN
    parseBool(details, Conf::CONFIG_STUN_ENABLE, stunEnabled_);
    parseString(details, Conf::CONFIG_STUN_SERVER, stunServer_);

    // ICE - TURN
    parseBool(details, Conf::CONFIG_TURN_ENABLE, turnEnabled_);
    parseString(details, Conf::CONFIG_TURN_SERVER, turnServer_);
    parseString(details, Conf::CONFIG_TURN_SERVER_UNAME, turnServerUserName_);
    parseString(details, Conf::CONFIG_TURN_SERVER_PWD, turnServerPwd_);
    parseString(details, Conf::CONFIG_TURN_SERVER_REALM, turnServerRealm_);
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
    a.emplace(Conf::CONFIG_PUBLISHED_PORT, std::to_string(publishedPort_));
    a.emplace(Conf::CONFIG_PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal_ ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_PUBLISHED_ADDRESS, publishedIpAddress_);
    a.emplace(Conf::CONFIG_STUN_ENABLE, stunEnabled_ ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_STUN_SERVER, stunServer_);
    a.emplace(Conf::CONFIG_TURN_ENABLE, turnEnabled_ ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_TURN_SERVER, turnServer_);
    a.emplace(Conf::CONFIG_TURN_SERVER_UNAME, turnServerUserName_);
    a.emplace(Conf::CONFIG_TURN_SERVER_PWD, turnServerPwd_);
    a.emplace(Conf::CONFIG_TURN_SERVER_REALM, turnServerRealm_);

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

IceTransportOptions
SIPAccountBase::getIceOptions() const noexcept
{
    IceTransportOptions opts;
    opts.upnpEnable = getUPnPActive();

    if (stunEnabled_)
        opts.stunServers.emplace_back(StunServerInfo().setUri(stunServer_));
    if (turnEnabled_) {
        auto cached = false;
        std::lock_guard<std::mutex> lk(cachedTurnMutex_);
        cached = cacheTurnV4_ || cacheTurnV6_;
        if (cacheTurnV4_ && *cacheTurnV4_) {
            opts.turnServers.emplace_back(TurnServerInfo()
                                              .setUri(cacheTurnV4_->toString(true))
                                              .setUsername(turnServerUserName_)
                                              .setPassword(turnServerPwd_)
                                              .setRealm(turnServerRealm_));
        }
        // NOTE: first test with ipv6 turn was not concluant and resulted in multiple
        // co issues. So this needs some debug. for now just disable
        // if (cacheTurnV6_ && *cacheTurnV6_) {
        //    opts.turnServers.emplace_back(TurnServerInfo()
        //                                      .setUri(cacheTurnV6_->toString(true))
        //                                      .setUsername(turnServerUserName_)
        //                                      .setPassword(turnServerPwd_)
        //                                      .setRealm(turnServerRealm_));
        //}
        // Nothing cached, so do the resolution
        if (!cached) {
            opts.turnServers.emplace_back(TurnServerInfo()
                                              .setUri(turnServer_)
                                              .setUsername(turnServerUserName_)
                                              .setPassword(turnServerPwd_)
                                              .setRealm(turnServerRealm_));
        }
    }
    return opts;
}

void
SIPAccountBase::onTextMessage(const std::string& id,
                              const std::string& from,
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
        if (m.first == MIME_TYPE_IM_COMPOSING) {
            try {
                static const std::regex COMPOSING_REGEX("<state>\\s*(\\w+)\\s*<\\/state>");
                std::smatch matched_pattern;
                std::regex_search(m.second, matched_pattern, COMPOSING_REGEX);
                bool isComposing {false};
                if (matched_pattern.ready() && !matched_pattern.empty()
                    && matched_pattern[1].matched) {
                    isComposing = matched_pattern[1] == "active";
                }
                static const std::regex CONVID_REGEX(
                    "<conversation>\\s*(\\w+)\\s*<\\/conversation>");
                std::regex_search(m.second, matched_pattern, CONVID_REGEX);
                std::string conversationId = "";
                if (matched_pattern.ready() && !matched_pattern.empty()
                    && matched_pattern[1].matched) {
                    conversationId = matched_pattern[1];
                }
                JAMI_WARN("@@@ %s", m.second.c_str());
                onIsComposing(conversationId, from, isComposing);
                if (payloads.size() == 1)
                    return;
            } catch (const std::exception& e) {
                JAMI_WARN("Error parsing composing state: %s", e.what());
            }
        } else if (m.first == MIME_TYPE_IMDN) {
            try {
                static const std::regex IMDN_MSG_ID_REGEX(
                    "<message-id>\\s*(\\w+)\\s*<\\/message-id>");
                static const std::regex IMDN_STATUS_REGEX("<status>\\s*<(\\w+)\\/>\\s*<\\/status>");
                std::smatch matched_pattern;

                std::regex_search(m.second, matched_pattern, IMDN_MSG_ID_REGEX);
                std::string messageId;
                if (matched_pattern.ready() && !matched_pattern.empty()
                    && matched_pattern[1].matched) {
                    messageId = matched_pattern[1];
                } else {
                    JAMI_WARN("Message displayed: can't parse message ID");
                    continue;
                }

                std::regex_search(m.second, matched_pattern, IMDN_STATUS_REGEX);
                bool isDisplayed {false};
                if (matched_pattern.ready() && !matched_pattern.empty()
                    && matched_pattern[1].matched) {
                    isDisplayed = matched_pattern[1] == "displayed";
                } else {
                    JAMI_WARN("Message displayed: can't parse status");
                    continue;
                }

                static const std::regex CONVID_REGEX(
                    "<conversation>\\s*(\\w+)\\s*<\\/conversation>");
                std::regex_search(m.second, matched_pattern, CONVID_REGEX);
                std::string conversationId = "";
                if (matched_pattern.ready() && !matched_pattern.empty()
                    && matched_pattern[1].matched) {
                    conversationId = matched_pattern[1];
                }

                if (conversationId.empty()) // Old method
                    messageEngine_.onMessageDisplayed(from, from_hex_string(messageId), isDisplayed);
                else if (isDisplayed) {
                    JAMI_DBG() << "[message " << messageId << "] Displayed by peer";
                    emitSignal<DRing::ConfigurationSignal::AccountMessageStatusChanged>(
                        accountID_,
                        conversationId,
                        from,
                        messageId,
                        static_cast<int>(DRing::Account::MessageStates::DISPLAYED));
                    return;
                }
                if (payloads.size() == 1)
                    return;
            } catch (const std::exception& e) {
                JAMI_WARN("Error parsing display notification: %s", e.what());
            }
        } else if (m.first == MIME_TYPE_GIT) {
            Json::Value json;
            std::string err;
            Json::CharReaderBuilder rbuilder;
            auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
            if (!reader->parse(m.second.data(), m.second.data() + m.second.size(), &json, &err)) {
                JAMI_ERR("Can't parse server response: %s", err.c_str());
                return;
            }

            JAMI_WARN("Received indication for new commit available in conversation %s",
                      json["id"].asString().c_str());

            onNewGitCommit(from,
                           json["deviceId"].asString(),
                           json["id"].asString(),
                           json["commit"].asString());
            return;
        } else if (m.first == MIME_TYPE_INVITE_JSON) {
            Json::Value json;
            std::string err;
            Json::CharReaderBuilder rbuilder;
            auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
            if (!reader->parse(m.second.data(), m.second.data() + m.second.size(), &json, &err)) {
                JAMI_ERR("Can't parse server response: %s", err.c_str());
                return;
            }
            onConversationRequest(from, json);
            return;
        } else if (m.first == MIME_TYPE_INVITE) {
            onNeedConversationRequest(from, m.second);
            return;
        } else if (m.first == MIME_TYPE_TEXT_PLAIN) {
            // This means that a text is received, so that
            // the conversation is not a swarm. For compatibility,
            // check if we have a swarm created. It can be the case
            // when the trust request confirm was not received.
            checkIfRemoveForCompat(from);
        }
    }

#ifdef ENABLE_PLUGIN
    auto& pluginChatManager
        = jami::Manager::instance().getJamiPluginManager().getChatServicesManager();
    std::shared_ptr<JamiMessage> cm = std::make_shared<JamiMessage>(
        accountID_, from, true, const_cast<std::map<std::string, std::string>&>(payloads), false);
    pluginChatManager.publishMessage(cm);
    emitSignal<DRing::ConfigurationSignal::IncomingAccountMessage>(accountID_, from, id, cm->data);

    DRing::Message message;
    message.from = from;
    message.payloads = cm->data;
#else
    emitSignal<DRing::ConfigurationSignal::IncomingAccountMessage>(accountID_, from, id, payloads);

    DRing::Message message;
    message.from = from;
    message.payloads = payloads;
#endif

    message.received = std::time(nullptr);
    std::lock_guard<std::mutex> lck(mutexLastMessages_);
    lastMessages_.emplace_back(std::move(message));
    while (lastMessages_.size() > MAX_WAITING_MESSAGES_SIZE) {
        lastMessages_.pop_front();
    }
}

bool
SIPAccountBase::setMessageDisplayed(const std::string& conversationUri,
                                    const std::string& messageId,
                                    int status)
{
    Uri uri(conversationUri);
    std::string conversationId = {};
    if (uri.scheme() == Uri::Scheme::SWARM)
        conversationId = uri.authority();
    if (status == (int) DRing::Account::MessageStates::DISPLAYED)
        sendInstantMessage(uri.authority(),
                           {{MIME_TYPE_IMDN, getDisplayed(conversationId, messageId)}});
    return true;
}

IpAddr
SIPAccountBase::getPublishedIpAddress(uint16_t family) const
{
    if (family == AF_INET)
        return publishedIp_[0];
    if (family == AF_INET6)
        return publishedIp_[1];

    assert(family == AF_UNSPEC);

    // If family is not set, prefere IPv4 if available. It's more
    // likely to succeed behind NAT.
    if (publishedIp_[0])
        return publishedIp_[0];
    if (publishedIp_[1])
        return publishedIp_[1];
    return {};
}

void
SIPAccountBase::setPublishedAddress(const IpAddr& ip_addr)
{
    if (ip_addr.getFamily() == AF_INET) {
        publishedIp_[0] = ip_addr;
    } else {
        publishedIp_[1] = ip_addr;
    }
}

std::vector<DRing::Message>
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
    bool secure = getSrtpKeyExchange() == KeyExchangeProtocol::SDES;

    // Add audio and DTMF events
    mediaList.emplace_back(
        MediaAttribute(MediaType::MEDIA_AUDIO, onHold, secure, false, "", "main audio"));

#ifdef ENABLE_VIDEO
    // Add video if allowed.
    if (isVideoEnabled() and addVideo) {
        mediaList.emplace_back(
            MediaAttribute(MediaType::MEDIA_VIDEO, onHold, secure, false, "", "main video"));
    }
#endif
    return mediaList;
}
} // namespace jami
