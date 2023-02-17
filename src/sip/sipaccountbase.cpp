/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

    auto status = pjsip_dlg_create_uac(pjsip_ua_instance(), from, contact, to, target, dlg);
    if (status != PJ_SUCCESS) {
        JAMI_ERR("Unable to create SIP dialogs for user agent client when calling %s %d",
                 to->ptr,
                 status);
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

void
SIPAccountBase::loadConfig()
{
    Account::loadConfig();
    const auto& conf = config();
    IpAddr publishedIp {conf.publishedIp};
    if (not conf.publishedSameasLocal and publishedIp)
        setPublishedAddress(publishedIp);
    TurnTransportParams turnParams;
    turnParams.domain = conf.turnServer;
    turnParams.username = conf.turnServerUserName;
    turnParams.password = conf.turnServerPwd;
    turnParams.realm = conf.turnServerRealm;
    if (!turnCache_) {
        auto cachePath = fileutils::get_cache_dir() + DIR_SEPARATOR_STR + getAccountID();
        turnCache_ = std::make_shared<TurnCache>(getAccountID(),
                                                 cachePath,
                                                 turnParams,
                                                 conf.turnEnabled);
    }
    turnCache_->reconfigure(turnParams, conf.turnEnabled);
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
                                     int details_code,
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
    return acquireRandomEvenPort(config().audioPortRange);
}

#ifdef ENABLE_VIDEO
uint16_t
SIPAccountBase::generateVideoPort() const
{
    return acquireRandomEvenPort(config().videoPortRange);
}
#endif

IceTransportOptions
SIPAccountBase::getIceOptions() const noexcept
{
    IceTransportOptions opts;
    opts.upnpEnable = getUPnPActive();

    // if (config().stunEnabled)
    //     opts.stunServers.emplace_back(StunServerInfo().setUri(stunServer_));
    if (config().turnEnabled && turnCache_) {
        auto turnAddr = turnCache_->getResolvedTurn();
        if (turnAddr != std::nullopt) {
            opts.turnServers.emplace_back(TurnServerInfo()
                                              .setUri(turnAddr->toString(true))
                                              .setUsername(config().turnServerUserName)
                                              .setPassword(config().turnServerPwd)
                                              .setRealm(config().turnServerRealm));
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
    }
    return opts;
}

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

IpAddr
SIPAccountBase::getPublishedIpAddress(uint16_t family) const
{
    if (family == AF_INET) {
        JAMI_ERROR("@@@ DAT {} {}", getAccountID(), publishedIp_[0].toString());
        return publishedIp_[0];
    }
    if (family == AF_INET6)
        return publishedIp_[1];

    assert(family == AF_UNSPEC);

    // If family is not set, prefere IPv4 if available. It's more
    // likely to succeed behind NAT.
    if (publishedIp_[0]) {
        JAMI_ERROR("@@@ DAT {} {}", getAccountID(), publishedIp_[0].toString());

        return publishedIp_[0];
    }
    if (publishedIp_[1])
        return publishedIp_[1];
    return {};
}

void
SIPAccountBase::setPublishedAddress(const IpAddr& ip_addr)
{
    if (ip_addr.getFamily() == AF_INET) {
        publishedIp_[0] = ip_addr;
        JAMI_ERROR("@@@ SET {} {}", getAccountID(), publishedIp_[0].toString());
    } else {
        publishedIp_[1] = ip_addr;
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
