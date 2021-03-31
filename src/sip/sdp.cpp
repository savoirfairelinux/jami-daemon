/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Eloi Bail <eloi.bail@savoirfairelinux.com>
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

#include "sdp.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sipaccount.h"
#include "sipvoiplink.h"
#include "string_utils.h"
#include "base64.h"

#include "manager.h"
#include "logger.h"
#include "libav_utils.h"
#include "array_size.h"

#include "media_codec.h"
#include "system_codec_container.h"
#include "compiler_intrinsics.h" // for UNUSED

#include <opendht/rng.h>
using random_device = dht::crypto::random_device;

#include <algorithm>
#include <cassert>

namespace jami {

using std::string;
using std::vector;
using std::stringstream;

static constexpr int POOL_INITIAL_SIZE = 16384;
static constexpr int POOL_INCREMENT_SIZE = POOL_INITIAL_SIZE;

Sdp::Sdp(const std::string& id)
    : memPool_(nullptr,
               [](pj_pool_t* pool) {
                   sip_utils::register_thread();
                   pj_pool_release(pool);
               })
    , publishedIpAddr_()
    , publishedIpAddrType_()
    , telephoneEventPayload_(101) // same as asterisk
    , sessionName_("Call ID " + id)
{
    sip_utils::register_thread();
    memPool_.reset(pj_pool_create(&Manager::instance().sipVoIPLink().getCachingPool()->factory,
                                  id.c_str(),
                                  POOL_INITIAL_SIZE,
                                  POOL_INCREMENT_SIZE,
                                  NULL));
    if (not memPool_)
        throw std::runtime_error("pj_pool_create() failed");
}

Sdp::~Sdp()
{
    SIPAccount::releasePort(localAudioDataPort_);
#ifdef ENABLE_VIDEO
    SIPAccount::releasePort(localVideoDataPort_);
#endif
}

std::shared_ptr<AccountCodecInfo>
Sdp::findCodecBySpec(const std::string& codec, const unsigned clockrate) const
{
    // TODO : only manage a list?
    for (const auto& accountCodec : audio_codec_list_) {
        auto audioCodecInfo = std::static_pointer_cast<AccountAudioCodecInfo>(accountCodec);
        auto& sysCodecInfo = *static_cast<const SystemAudioCodecInfo*>(
            &audioCodecInfo->systemCodecInfo);
        if (sysCodecInfo.name.compare(codec) == 0
            and (audioCodecInfo->isPCMG722() ? (clockrate == 8000)
                                             : (sysCodecInfo.audioformat.sample_rate == clockrate)))
            return accountCodec;
    }

    for (const auto& accountCodec : video_codec_list_) {
        auto sysCodecInfo = accountCodec->systemCodecInfo;
        if (sysCodecInfo.name.compare(codec) == 0)
            return accountCodec;
    }
    return nullptr;
}

std::shared_ptr<AccountCodecInfo>
Sdp::findCodecByPayload(const unsigned payloadType)
{
    // TODO : only manage a list?
    for (const auto& accountCodec : audio_codec_list_) {
        auto sysCodecInfo = accountCodec->systemCodecInfo;
        if (sysCodecInfo.payloadType == payloadType)
            return accountCodec;
    }

    for (const auto& accountCodec : video_codec_list_) {
        auto sysCodecInfo = accountCodec->systemCodecInfo;
        if (sysCodecInfo.payloadType == payloadType)
            return accountCodec;
    }
    return nullptr;
}

static void
randomFill(std::vector<uint8_t>& dest)
{
    std::uniform_int_distribution<int> rand_byte {0, std::numeric_limits<uint8_t>::max()};
    random_device rdev;
    std::generate(dest.begin(), dest.end(), std::bind(rand_byte, std::ref(rdev)));
}

void
Sdp::setActiveLocalSdpSession(const pjmedia_sdp_session* sdp)
{
    activeLocalSession_ = sdp;
}

void
Sdp::setActiveRemoteSdpSession(const pjmedia_sdp_session* sdp)
{
    activeRemoteSession_ = sdp;
}

pjmedia_sdp_attr*
Sdp::generateSdesAttribute()
{
    static constexpr const unsigned cryptoSuite = 0;
    std::vector<uint8_t> keyAndSalt;
    keyAndSalt.resize(jami::CryptoSuites[cryptoSuite].masterKeyLength / 8
                      + jami::CryptoSuites[cryptoSuite].masterSaltLength / 8);
    // generate keys
    randomFill(keyAndSalt);

    std::string crypto_attr = "1 "s + jami::CryptoSuites[cryptoSuite].name
                              + " inline:" + base64::encode(keyAndSalt);
    pj_str_t val {sip_utils::CONST_PJ_STR(crypto_attr)};
    return pjmedia_sdp_attr_create(memPool_.get(), "crypto", &val);
}

// Legacy direction inference
char const*
Sdp::mediaDirection(MediaType type, bool onHold)
{
    return onHold ? (type == MediaType::MEDIA_AUDIO ? "sendonly" : "inactive") : "sendrecv";
}

// Direction inference based on RFC-3264 and RFC-6337
char const*
Sdp::mediaDirection(const MediaAttribute& localAttr, const MediaAttribute& remoteAttr)
{
    if (not localAttr.enabled_ or not remoteAttr.enabled_)
        return "inactive";

    if (localAttr.muted_) {
        if (remoteAttr.muted_)
            return "inactive";
        else
            return "recvonly";
    }

    // Local is enabled and not muted. Check remote.
    if (remoteAttr.muted_)
        return "sendonly";

    // Both enabled and not muted.
    return "sendrecv";
}

MediaDirection
Sdp::getMediaDirection(pjmedia_sdp_media* media)
{
    if (pjmedia_sdp_attr_find2(media->attr_count, media->attr, "sendrecv", nullptr) != nullptr) {
        return MediaDirection::SENDRECV;
    }

    if (pjmedia_sdp_attr_find2(media->attr_count, media->attr, "sendonly", nullptr) != nullptr) {
        return MediaDirection::SENDONLY;
    }

    if (pjmedia_sdp_attr_find2(media->attr_count, media->attr, "recvonly", nullptr) != nullptr) {
        return MediaDirection::RECVONLY;
    }

    if (pjmedia_sdp_attr_find2(media->attr_count, media->attr, "inactive", nullptr) != nullptr) {
        return MediaDirection::INACTIVE;
    }

    return MediaDirection::UNKNOWN;
}

MediaTransport
Sdp::getMediaTransport(pjmedia_sdp_media* media)
{
    if (pj_stricmp2(&media->desc.transport, "RTP/SAVP") == 0)
        return MediaTransport::RTP_SAVP;
    else if (pj_stricmp2(&media->desc.transport, "RTP/AVP") == 0)
        return MediaTransport::RTP_AVP;

    return MediaTransport::UNKNOWN;
}

std::vector<std::string>
Sdp::getCrypto(pjmedia_sdp_media* media)
{
    std::vector<std::string> crypto;
    for (unsigned j = 0; j < media->attr_count; j++) {
        const auto attribute = media->attr[j];
        if (pj_stricmp2(&attribute->name, "crypto") == 0)
            crypto.emplace_back(attribute->value.ptr, attribute->value.slen);
    }

    return crypto;
}

pjmedia_sdp_media*
Sdp::addMediaDescription(const MediaAttribute& mediaAttr, bool onHold)
{
    auto type = mediaAttr.type_;
    auto secure = mediaAttr.secure_;

    JAMI_DBG("Add media description [%s]", mediaAttr.toString(true).c_str());

    pjmedia_sdp_media* med = PJ_POOL_ZALLOC_T(memPool_.get(), pjmedia_sdp_media);

    switch (type) {
    case MediaType::MEDIA_AUDIO:
        med->desc.media = sip_utils::CONST_PJ_STR("audio");
        med->desc.port = localAudioDataPort_;
        med->desc.fmt_count = audio_codec_list_.size();
        break;
    case MediaType::MEDIA_VIDEO:
        med->desc.media = sip_utils::CONST_PJ_STR("video");
        med->desc.port = localVideoDataPort_;
        med->desc.fmt_count = video_codec_list_.size();
        break;
    default:
        throw SdpException("Unsupported media type! Only audio and video are supported");
        break;
    }

    med->desc.port_count = 1;

    // Set the transport protocol of the media
    med->desc.transport = secure ? sip_utils::CONST_PJ_STR("RTP/SAVP")
                                 : sip_utils::CONST_PJ_STR("RTP/AVP");

    unsigned dynamic_payload = 96;

    for (unsigned i = 0; i < med->desc.fmt_count; i++) {
        pjmedia_sdp_rtpmap rtpmap;
        rtpmap.param.slen = 0;

        std::string channels; // must have the lifetime of rtpmap
        std::string enc_name;
        unsigned payload;

        if (type == MediaType::MEDIA_AUDIO) {
            auto accountAudioCodec = std::static_pointer_cast<AccountAudioCodecInfo>(
                audio_codec_list_[i]);
            payload = accountAudioCodec->payloadType;
            enc_name = accountAudioCodec->systemCodecInfo.name;

            if (accountAudioCodec->audioformat.nb_channels > 1) {
                channels = std::to_string(accountAudioCodec->audioformat.nb_channels);
                rtpmap.param = sip_utils::CONST_PJ_STR(channels);
            }
            // G722 requires G722/8000 media description even though it's @ 16000 Hz
            // See http://tools.ietf.org/html/rfc3551#section-4.5.2
            if (accountAudioCodec->isPCMG722())
                rtpmap.clock_rate = 8000;
            else
                rtpmap.clock_rate = accountAudioCodec->audioformat.sample_rate;

        } else {
            // FIXME: get this key from header
            payload = dynamic_payload++;
            enc_name = video_codec_list_[i]->systemCodecInfo.name;
            rtpmap.clock_rate = 90000;
        }

        auto payloadStr = std::to_string(payload);
        auto pjPayload = sip_utils::CONST_PJ_STR(payloadStr);
        pj_strdup(memPool_.get(), &med->desc.fmt[i], &pjPayload);

        // Add a rtpmap field for each codec
        // We could add one only for dynamic payloads because the codecs with static RTP payloads
        // are entirely defined in the RFC 3351
        rtpmap.pt = med->desc.fmt[i];
        rtpmap.enc_name = sip_utils::CONST_PJ_STR(enc_name);

        pjmedia_sdp_attr* attr;
        pjmedia_sdp_rtpmap_to_attr(memPool_.get(), &rtpmap, &attr);
        med->attr[med->attr_count++] = attr;

#ifdef ENABLE_VIDEO
        if (enc_name == "H264") {
            // FIXME: this should not be hardcoded, it will determine what profile and level
            // our peer will send us
            const auto accountVideoCodec = std::static_pointer_cast<AccountVideoCodecInfo>(
                video_codec_list_[i]);
            const auto& profileLevelID = accountVideoCodec->parameters.empty()
                                             ? libav_utils::DEFAULT_H264_PROFILE_LEVEL_ID
                                             : accountVideoCodec->parameters;
            auto value = fmt::format("fmtp:{} {}", payload, profileLevelID);
            med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_.get(),
                                                                   value.c_str(),
                                                                   NULL);
        }
#endif
    }

    if (type == MediaType::MEDIA_AUDIO) {
        setTelephoneEventRtpmap(med);
        addRTCPAttribute(med); // video has its own RTCP
    }

    char const* direction = mediaDirection(type, onHold);

    med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_.get(), direction, NULL);

    if (secure) {
        if (pjmedia_sdp_media_add_attr(med, generateSdesAttribute()) != PJ_SUCCESS)
            throw SdpException("Could not add sdes attribute to media");
    }

    return med;
}

void
Sdp::addRTCPAttribute(pjmedia_sdp_media* med)
{
    IpAddr outputAddr {publishedIpAddr_};
    outputAddr.setPort(localAudioControlPort_);
    pjmedia_sdp_attr* attr = pjmedia_sdp_attr_create_rtcp(memPool_.get(), outputAddr.pjPtr());
    if (attr)
        pjmedia_sdp_attr_add(&med->attr_count, med->attr, attr);
}

void
Sdp::setPublishedIP(const std::string& addr, pj_uint16_t addr_type)
{
    publishedIpAddr_ = addr;
    publishedIpAddrType_ = addr_type;
    if (localSession_) {
        if (addr_type == pj_AF_INET6())
            localSession_->origin.addr_type = sip_utils::CONST_PJ_STR("IP6");
        else
            localSession_->origin.addr_type = sip_utils::CONST_PJ_STR("IP4");
        localSession_->origin.addr = sip_utils::CONST_PJ_STR(publishedIpAddr_);
        localSession_->conn->addr = localSession_->origin.addr;
        if (pjmedia_sdp_validate(localSession_) != PJ_SUCCESS)
            JAMI_ERR("Could not validate SDP");
    }
}

void
Sdp::setPublishedIP(const IpAddr& ip_addr)
{
    setPublishedIP(ip_addr, ip_addr.getFamily());
}

void
Sdp::setTelephoneEventRtpmap(pjmedia_sdp_media* med)
{
    ++med->desc.fmt_count;
    pj_strdup2(memPool_.get(),
               &med->desc.fmt[med->desc.fmt_count - 1],
               std::to_string(telephoneEventPayload_).c_str());

    pjmedia_sdp_attr* attr_rtpmap = static_cast<pjmedia_sdp_attr*>(
        pj_pool_zalloc(memPool_.get(), sizeof(pjmedia_sdp_attr)));
    attr_rtpmap->name = sip_utils::CONST_PJ_STR("rtpmap");
    attr_rtpmap->value = sip_utils::CONST_PJ_STR("101 telephone-event/8000");

    med->attr[med->attr_count++] = attr_rtpmap;

    pjmedia_sdp_attr* attr_fmtp = static_cast<pjmedia_sdp_attr*>(
        pj_pool_zalloc(memPool_.get(), sizeof(pjmedia_sdp_attr)));
    attr_fmtp->name = sip_utils::CONST_PJ_STR("fmtp");
    attr_fmtp->value = sip_utils::CONST_PJ_STR("101 0-15");

    med->attr[med->attr_count++] = attr_fmtp;
}

void
Sdp::setLocalMediaCapabilities(MediaType type,
                               const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedCodecs)
{
    switch (type) {
    case MediaType::MEDIA_AUDIO:
        audio_codec_list_ = selectedCodecs;
        break;

    case MediaType::MEDIA_VIDEO:
#ifdef ENABLE_VIDEO
        video_codec_list_ = selectedCodecs;
        // Do not expose H265 if accel is disactivated
        if (not jami::Manager::instance().videoPreferences.getEncodingAccelerated()) {
            video_codec_list_.erase(std::remove_if(video_codec_list_.begin(),
                                                   video_codec_list_.end(),
                                                   [](const std::shared_ptr<AccountCodecInfo>& i) {
                                                       return i->systemCodecInfo.name == "H265";
                                                   }),
                                    video_codec_list_.end());
        }
#else
        (void) selectedCodecs;
#endif
        break;

    default:
        throw SdpException("Unsupported media type");
        break;
    }
}

const char*
Sdp::getSdpDirectionStr(SdpDirection direction)
{
    if (direction == SdpDirection::LOCAL_OFFER)
        return "LOCAL_OFFER";
    if (direction == SdpDirection::LOCAL_ANSWER)
        return "LOCAL_ANSWER";
    if (direction == SdpDirection::REMOTE_OFFER)
        return "REMOTE_OFFER";
    if (direction == SdpDirection::REMOTE_ANSWER)
        return "REMOTE_ANSWER";
    return "NONE";
}

void
Sdp::printSession(const pjmedia_sdp_session* session, const char* header, SdpDirection direction)
{
    static constexpr size_t BUF_SZ = 4095;
    sip_utils::register_thread();
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&>
        tmpPool_(pj_pool_create(&Manager::instance().sipVoIPLink().getCachingPool()->factory,
                                "printSdp",
                                BUF_SZ,
                                BUF_SZ,
                                nullptr),
                 pj_pool_release);

    auto cloned_session = pjmedia_sdp_session_clone(tmpPool_.get(), session);
    if (!cloned_session) {
        JAMI_ERR("Could not clone SDP for printing");
        return;
    }

    // Filter-out sensible data like SRTP master key.
    for (unsigned i = 0; i < cloned_session->media_count; ++i) {
        pjmedia_sdp_media_remove_all_attr(cloned_session->media[i], "crypto");
    }

    std::array<char, BUF_SZ + 1> buffer;
    auto size = pjmedia_sdp_print(cloned_session, buffer.data(), BUF_SZ);
    if (size < 0) {
        JAMI_ERR("%s SDP too big for dump", header);
        return;
    }

    JAMI_DBG("[SDP %s] %s\n%.*s", getSdpDirectionStr(direction), header, size, buffer.data());
}

void
Sdp::createLocalSession(SdpDirection direction)
{
    sdpDirection_ = direction;
    localSession_ = PJ_POOL_ZALLOC_T(memPool_.get(), pjmedia_sdp_session);
    localSession_->conn = PJ_POOL_ZALLOC_T(memPool_.get(), pjmedia_sdp_conn);

    /* Initialize the fields of the struct */
    localSession_->origin.version = 0;
    pj_time_val tv;
    pj_gettimeofday(&tv);

    localSession_->origin.user = *pj_gethostname();

    // Use Network Time Protocol format timestamp to ensure uniqueness.
    localSession_->origin.id = tv.sec + 2208988800UL;
    localSession_->origin.net_type = sip_utils::CONST_PJ_STR("IN");
    if (publishedIpAddrType_ == pj_AF_INET6())
        localSession_->origin.addr_type = sip_utils::CONST_PJ_STR("IP6");
    else
        localSession_->origin.addr_type = sip_utils::CONST_PJ_STR("IP4");
    localSession_->origin.addr = sip_utils::CONST_PJ_STR(publishedIpAddr_);

    // Use the call IDs for s= line
    localSession_->name = sip_utils::CONST_PJ_STR(sessionName_);

    localSession_->conn->net_type = localSession_->origin.net_type;
    localSession_->conn->addr_type = localSession_->origin.addr_type;
    localSession_->conn->addr = localSession_->origin.addr;

    // RFC 3264: An offer/answer model session description protocol
    // As the session is created and destroyed through an external signaling mean (SIP), the line
    // should have a value of "0 0".
    localSession_->time.start = 0;
    localSession_->time.stop = 0;
}

int
Sdp::validateSession() const
{
    return pjmedia_sdp_validate(localSession_);
}

bool
Sdp::createOffer(const std::vector<MediaAttribute>& mediaList)
{
    if (mediaList.size() >= PJMEDIA_MAX_SDP_MEDIA) {
        throw SdpException("Media list size exceeds SDP media maximum size");
    }

    JAMI_DBG("Creating SDP offer with %lu medias", mediaList.size());

    createLocalSession(SdpDirection::LOCAL_OFFER);

    if (validateSession() != PJ_SUCCESS) {
        JAMI_ERR("Failed to create initial offer");
        return false;
    }

    localSession_->media_count = 0;

    for (auto const& media : mediaList) {
        localSession_->media[localSession_->media_count++] = addMediaDescription(media);
    }

    if (validateSession() != PJ_SUCCESS) {
        JAMI_ERR("Failed to add medias");
        return false;
    }

    if (pjmedia_sdp_neg_create_w_local_offer(memPool_.get(), localSession_, &negotiator_)
        != PJ_SUCCESS) {
        JAMI_ERR("Failed to create an initial SDP negotiator");
        return false;
    }

    Sdp::printSession(localSession_, "Local session (initial):", sdpDirection_);

    return true;
}

void
Sdp::setReceivedOffer(const pjmedia_sdp_session* remote)
{
    if (remote == nullptr) {
        JAMI_ERR("Remote session is NULL");
        return;
    }
    remoteSession_ = pjmedia_sdp_session_clone(memPool_.get(), remote);
}

bool
Sdp::processIncomingOffer(const std::vector<MediaAttribute>& mediaList)
{
    assert(remoteSession_);

    JAMI_DBG("Processing received offer for [%s] with %lu", sessionName_.c_str(), mediaList.size());

    printSession(remoteSession_, "Remote session:", SdpDirection::REMOTE_OFFER);

    if (not localSession_) {
        createLocalSession(SdpDirection::LOCAL_ANSWER);
        if (validateSession() != PJ_SUCCESS) {
            JAMI_ERR("Failed to create initial offer");
            return false;
        }
    }

    localSession_->media_count = 0;

    for (auto const& media : mediaList) {
        localSession_->media[localSession_->media_count++] = addMediaDescription(media);
    }

    printSession(localSession_, "Local session:\n", sdpDirection_);

    if (validateSession() != PJ_SUCCESS) {
        JAMI_ERR("Failed to add medias");
        return false;
    }

    if (pjmedia_sdp_neg_create_w_remote_offer(memPool_.get(),
                                              localSession_,
                                              remoteSession_,
                                              &negotiator_)
        != PJ_SUCCESS) {
        JAMI_ERR("Failed to initialize media negotiation");
        return false;
    }

    return true;
}

bool
Sdp::startNegotiation()
{
    JAMI_DBG("Starting media negotiation for [%s]", sessionName_.c_str());

    if (negotiator_ == NULL) {
        JAMI_ERR("Can't start negotiation with invalid negotiator");
        return false;
    }

    const pjmedia_sdp_session* active_local;
    const pjmedia_sdp_session* active_remote;

    if (pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO) {
        JAMI_WARN("Negotiator not in right state for negotiation");
        return false;
    }

    if (pjmedia_sdp_neg_negotiate(memPool_.get(), negotiator_, 0) != PJ_SUCCESS) {
        JAMI_ERR("Failed to start media negotiation");
        return false;
    }

    if (pjmedia_sdp_neg_get_active_local(negotiator_, &active_local) != PJ_SUCCESS)
        JAMI_ERR("Could not retrieve local active session");
    return false;

    setActiveLocalSdpSession(active_local);

    if (active_local != nullptr) {
        Sdp::printSession(active_local, "Local active session:", sdpDirection_);
        return false;
    }

    if (pjmedia_sdp_neg_get_active_remote(negotiator_, &active_remote) != PJ_SUCCESS
        or active_remote == nullptr) {
        JAMI_ERR("Could not retrieve remote active session");
        return false;
    }

    setActiveRemoteSdpSession(active_remote);

    Sdp::printSession(active_remote, "Remote active session:", sdpDirection_);

    return true;
}

std::string
Sdp::getFilteredSdp(const pjmedia_sdp_session* session, unsigned media_keep, unsigned pt_keep)
{
    sip_utils::register_thread();
    static constexpr size_t BUF_SZ = 4096;
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&>
        tmpPool_(pj_pool_create(&Manager::instance().sipVoIPLink().getCachingPool()->factory,
                                "tmpSdp",
                                BUF_SZ,
                                BUF_SZ,
                                nullptr),
                 pj_pool_release);
    auto cloned = pjmedia_sdp_session_clone(tmpPool_.get(), session);
    if (!cloned) {
        JAMI_ERR("Could not clone SDP");
        return "";
    }

    // deactivate non-video media
    bool hasKeep = false;
    for (unsigned i = 0; i < cloned->media_count; i++)
        if (i != media_keep) {
            if (pjmedia_sdp_media_deactivate(tmpPool_.get(), cloned->media[i]) != PJ_SUCCESS)
                JAMI_ERR("Could not deactivate media");
        } else {
            hasKeep = true;
        }

    if (not hasKeep) {
        JAMI_DBG("No media to keep present in SDP");
        return "";
    }

    // Leaking medias will be dropped with tmpPool_
    for (unsigned i = 0; i < cloned->media_count; i++)
        if (cloned->media[i]->desc.port == 0) {
            std::move(cloned->media + i + 1, cloned->media + cloned->media_count, cloned->media + i);
            cloned->media_count--;
            i--;
        }

    for (unsigned i = 0; i < cloned->media_count; i++) {
        auto media = cloned->media[i];

        // filter other codecs
        for (unsigned c = 0; c < media->desc.fmt_count; c++) {
            auto& pt = media->desc.fmt[c];
            if (pj_strtoul(&pt) == pt_keep)
                continue;

            while (auto attr = pjmedia_sdp_attr_find2(media->attr_count, media->attr, "rtpmap", &pt))
                pjmedia_sdp_attr_remove(&media->attr_count, media->attr, attr);

            while (auto attr = pjmedia_sdp_attr_find2(media->attr_count, media->attr, "fmt", &pt))
                pjmedia_sdp_attr_remove(&media->attr_count, media->attr, attr);

            std::move(media->desc.fmt + c + 1,
                      media->desc.fmt + media->desc.fmt_count,
                      media->desc.fmt + c);
            media->desc.fmt_count--;
            c--;
        }

        // we handle crypto ourselfs, don't tell libav about it
        pjmedia_sdp_media_remove_all_attr(media, "crypto");
    }

    char buffer[BUF_SZ];
    size_t size = pjmedia_sdp_print(cloned, buffer, sizeof(buffer));
    string sessionStr(buffer, std::min(size, sizeof(buffer)));

    return sessionStr;
}

std::vector<MediaDescription>
Sdp::getActiveMediaDescription(bool remote) const
{
    if (remote)
        return getMediaDescriptions(activeRemoteSession_, true);

    return getMediaDescriptions(activeLocalSession_, false);
}

std::vector<MediaDescription>
Sdp::getMediaDescriptions(const pjmedia_sdp_session* session, bool remote) const
{
    if (!session)
        return {};
    static constexpr pj_str_t STR_RTPMAP {sip_utils::CONST_PJ_STR("rtpmap")};
    static constexpr pj_str_t STR_FMTP {sip_utils::CONST_PJ_STR("fmtp")};

    std::vector<MediaDescription> ret;
    for (unsigned i = 0; i < session->media_count; i++) {
        auto media = session->media[i];
        ret.emplace_back(MediaDescription());
        MediaDescription& descr = ret.back();
        if (!pj_stricmp2(&media->desc.media, "audio"))
            descr.type = MEDIA_AUDIO;
        else if (!pj_stricmp2(&media->desc.media, "video"))
            descr.type = MEDIA_VIDEO;
        else
            continue;

        descr.enabled = media->desc.port;
        if (!descr.enabled)
            continue;

        // get connection info
        pjmedia_sdp_conn* conn = media->conn ? media->conn : session->conn;
        if (not conn) {
            JAMI_ERR("Could not find connection information for media");
            continue;
        }
        descr.addr = std::string_view(conn->addr.ptr, conn->addr.slen);
        descr.addr.setPort(media->desc.port);

        // Get the "rtcp" address from the SDP if present. Otherwise,
        // infere it from endpoint (RTP) address.
        auto attr = pjmedia_sdp_attr_find2(media->attr_count, media->attr, "rtcp", NULL);
        if (attr) {
            pjmedia_sdp_rtcp_attr rtcp;
            auto status = pjmedia_sdp_attr_get_rtcp(attr, &rtcp);
            if (status == PJ_SUCCESS && rtcp.addr.slen) {
                descr.rtcp_addr = std::string_view(rtcp.addr.ptr, rtcp.addr.slen);
                descr.rtcp_addr.setPort(rtcp.port);
            }
        }

        descr.onHold = pjmedia_sdp_attr_find2(media->attr_count, media->attr, "sendonly", nullptr)
                       || pjmedia_sdp_attr_find2(media->attr_count,
                                                 media->attr,
                                                 "inactive",
                                                 nullptr);

        descr.direction_ = getMediaDirection(media);
        if (descr.direction_ == MediaDirection::UNKNOWN) {
            JAMI_ERR("Did not find media direction attribute in remote SDP");
        }

        // get codecs infos
        for (unsigned j = 0; j < media->desc.fmt_count; j++) {
            const auto rtpMapAttribute = pjmedia_sdp_media_find_attr(media,
                                                                     &STR_RTPMAP,
                                                                     &media->desc.fmt[j]);
            if (!rtpMapAttribute) {
                JAMI_ERR("Could not find rtpmap attribute");
                descr.enabled = false;
                continue;
            }
            pjmedia_sdp_rtpmap rtpmap;
            if (pjmedia_sdp_attr_get_rtpmap(rtpMapAttribute, &rtpmap) != PJ_SUCCESS
                || rtpmap.enc_name.slen == 0) {
                JAMI_ERR("Could not find payload type %.*s in SDP",
                         (int) media->desc.fmt[j].slen,
                         media->desc.fmt[j].ptr);
                descr.enabled = false;
                continue;
            }
            const std::string codec_raw(rtpmap.enc_name.ptr, rtpmap.enc_name.slen);
            descr.rtp_clockrate = rtpmap.clock_rate;
            descr.codec = findCodecBySpec(codec_raw, rtpmap.clock_rate);
            if (not descr.codec) {
                JAMI_ERR("Could not find codec %s", codec_raw.c_str());
                descr.enabled = false;
                continue;
            }
            descr.payload_type = pj_strtoul(&rtpmap.pt);
            if (descr.type == MEDIA_VIDEO) {
                const auto fmtpAttr = pjmedia_sdp_media_find_attr(media,
                                                                  &STR_FMTP,
                                                                  &media->desc.fmt[j]);
                // descr.bitrate = getOutgoingVideoField(codec, "bitrate");
                if (fmtpAttr && fmtpAttr->value.ptr && fmtpAttr->value.slen) {
                    const auto& v = fmtpAttr->value;
                    descr.parameters = std::string(v.ptr, v.ptr + v.slen);
                }
            }
            // for now, just keep the first codec only
            descr.enabled = true;
            break;
        }

        if (not remote)
            descr.receiving_sdp = getFilteredSdp(session, i, descr.payload_type);

        // get crypto info
        std::vector<std::string> crypto;
        for (unsigned j = 0; j < media->attr_count; j++) {
            const auto attribute = media->attr[j];
            if (pj_stricmp2(&attribute->name, "crypto") == 0)
                crypto.emplace_back(attribute->value.ptr, attribute->value.slen);
        }
        descr.crypto = SdesNegotiator::negotiate(crypto);
    }
    return ret;
}

std::vector<Sdp::MediaSlot>
Sdp::getMediaSlots() const
{
    auto loc = getMediaDescriptions(activeLocalSession_, false);
    auto rem = getMediaDescriptions(activeRemoteSession_, true);
    size_t slot_n = std::min(loc.size(), rem.size());
    std::vector<MediaSlot> s;
    s.reserve(slot_n);
    for (decltype(slot_n) i = 0; i < slot_n; i++)
        s.emplace_back(std::move(loc[i]), std::move(rem[i]));
    return s;
}

void
Sdp::addIceCandidates(unsigned media_index, const std::vector<std::string>& cands)
{
    if (media_index >= localSession_->media_count) {
        JAMI_ERR("addIceCandidates failed: cannot access media#%u (may be deactivated)",
                 media_index);
        return;
    }

    auto media = localSession_->media[media_index];

    for (const auto& item : cands) {
        const pj_str_t val = sip_utils::CONST_PJ_STR(item);
        pjmedia_sdp_attr* attr = pjmedia_sdp_attr_create(memPool_.get(), "candidate", &val);

        if (pjmedia_sdp_media_add_attr(media, attr) != PJ_SUCCESS)
            throw SdpException("Could not add ICE candidates attribute to media");
    }
}

std::vector<std::string>
Sdp::getIceCandidates(unsigned media_index) const
{
    auto session = activeRemoteSession_ ? activeRemoteSession_ : remoteSession_;
    auto localSession = activeLocalSession_ ? activeLocalSession_ : localSession_;
    if (not session) {
        JAMI_ERR("getIceCandidates failed: no remote session");
        return {};
    }
    if (not localSession) {
        JAMI_ERR("getIceCandidates failed: no local session");
        return {};
    }
    if (media_index >= session->media_count || media_index >= localSession->media_count) {
        JAMI_ERR("getIceCandidates failed: cannot access media#%u (may be deactivated)",
                 media_index);
        return {};
    }
    auto media = session->media[media_index];
    auto localMedia = localSession->media[media_index];
    if (media->desc.port == 0 || localMedia->desc.port == 0) {
        JAMI_WARN("Media#%u is disabled. Media ports: local %u, remote %u",
                  media_index,
                  localMedia->desc.port,
                  media->desc.port);
        return {};
    }

    std::vector<std::string> candidates;

    for (unsigned i = 0; i < media->attr_count; i++) {
        pjmedia_sdp_attr* attribute = media->attr[i];
        if (pj_stricmp2(&attribute->name, "candidate") == 0)
            candidates.push_back(std::string(attribute->value.ptr, attribute->value.slen));
    }

    return candidates;
}

void
Sdp::addIceAttributes(const IceTransport::Attribute&& ice_attrs)
{
    pj_str_t value = sip_utils::CONST_PJ_STR(ice_attrs.ufrag);
    pjmedia_sdp_attr* attr = pjmedia_sdp_attr_create(memPool_.get(), "ice-ufrag", &value);

    if (pjmedia_sdp_attr_add(&localSession_->attr_count, localSession_->attr, attr) != PJ_SUCCESS)
        throw SdpException("Could not add ICE.ufrag attribute to local SDP");

    value = sip_utils::CONST_PJ_STR(ice_attrs.pwd);
    attr = pjmedia_sdp_attr_create(memPool_.get(), "ice-pwd", &value);

    if (pjmedia_sdp_attr_add(&localSession_->attr_count, localSession_->attr, attr) != PJ_SUCCESS)
        throw SdpException("Could not add ICE.pwd attribute to local SDP");
}

IceTransport::Attribute
Sdp::getIceAttributes() const
{
    IceTransport::Attribute ice_attrs;
    if (auto session = (activeRemoteSession_ ? activeRemoteSession_ : remoteSession_))
        return getIceAttributes(session);
    return {};
}

IceTransport::Attribute
Sdp::getIceAttributes(const pjmedia_sdp_session* session)
{
    IceTransport::Attribute ice_attrs;
    for (unsigned i = 0; i < session->attr_count; i++) {
        pjmedia_sdp_attr* attribute = session->attr[i];
        if (pj_stricmp2(&attribute->name, "ice-ufrag") == 0)
            ice_attrs.ufrag.assign(attribute->value.ptr, attribute->value.slen);
        else if (pj_stricmp2(&attribute->name, "ice-pwd") == 0)
            ice_attrs.pwd.assign(attribute->value.ptr, attribute->value.slen);
    }
    return ice_attrs;
}

void
Sdp::clearIce()
{
    clearIce(localSession_);
    clearIce(remoteSession_);
}

void
Sdp::clearIce(pjmedia_sdp_session* session)
{
    if (not session)
        return;
    pjmedia_sdp_attr_remove_all(&session->attr_count, session->attr, "ice-ufrag");
    pjmedia_sdp_attr_remove_all(&session->attr_count, session->attr, "ice-pwd");
    pjmedia_sdp_attr_remove_all(&session->attr_count, session->attr, "candidate");
    for (unsigned i = 0; i < session->media_count; i++) {
        auto media = session->media[i];
        pjmedia_sdp_attr_remove_all(&media->attr_count, media->attr, "candidate");
    }
}

std::vector<MediaAttribute>
Sdp::getMediaAttributeListFromSdp(const pjmedia_sdp_session* sdpSession)
{
    std::vector<MediaAttribute> mediaList;
    assert(sdpSession != nullptr);
    for (unsigned idx = 0; idx < sdpSession->media_count; idx++) {
        mediaList.emplace_back(MediaAttribute {});
        auto& mediaAttr = mediaList.back();

        auto const& media = sdpSession->media[idx];

        // Get media type.
        if (!pj_stricmp2(&media->desc.media, "audio"))
            mediaAttr.type_ = MediaType::MEDIA_AUDIO;
        else if (!pj_stricmp2(&media->desc.media, "video"))
            mediaAttr.type_ = MediaType::MEDIA_VIDEO;
        else {
            JAMI_WARN("Media#%u only 'audio' and 'video' types are supported!", idx);
            // Keep the media in the list but dont bother parsing the attributes
            continue;
        }

        // Get mute state.
        auto direction = getMediaDirection(media);
        mediaAttr.muted_ = direction != MediaDirection::SENDRECV
                           and direction != MediaDirection::SENDONLY;

        // Get transport.
        auto transp = getMediaTransport(media);
        if (transp == MediaTransport::UNKNOWN) {
            JAMI_WARN("Media#%u could not determine transport type!", idx);
        }

        // A media is secure if the transport is of type RTP/SAVP
        // and the crypto materials are present.
        mediaAttr.secure_ = transp == MediaTransport::RTP_SAVP and not getCrypto(media).empty();

        mediaAttr.label_ = mediaAttr.type_ == MediaType::MEDIA_AUDIO ? "audio_" : "video_";
        mediaAttr.label_ += std::to_string(idx);
    }

    return mediaList;
}

} // namespace jami
