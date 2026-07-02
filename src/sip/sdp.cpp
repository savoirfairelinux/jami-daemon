/*
 * Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "sdp.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sip/sipaccount.h"
#include "sip/sipvoiplink.h"
#include "string_utils.h"
#include "base64.h"

#include "manager.h"
#include "logger.h"
#include "libav_utils.h"
#include "media/h264_profile.h"
#include "media/h265_profile.h"
#include "jami/media_const.h"

#include "media_codec.h"
#include "sdes_negotiator.h"

#include <opendht/rng.h>

#include <algorithm>
#include <cassert>
#include <mutex>

namespace jami {

using std::string;
using std::vector;

static constexpr int POOL_INITIAL_SIZE = 16384;
static constexpr int POOL_INCREMENT_SIZE = POOL_INITIAL_SIZE;

static std::map<MediaDirection, const char*> DIRECTION_STR {{MediaDirection::SENDRECV, "sendrecv"},
                                                            {MediaDirection::SENDONLY, "sendonly"},
                                                            {MediaDirection::RECVONLY, "recvonly"},
                                                            {MediaDirection::INACTIVE, "inactive"}};

#ifdef ENABLE_VIDEO
/**
 * RFC 6184 §8.2.2: within an offered H.264 payload type, the profile part of
 * profile-level-id is symmetric; the answer must use the same profile.
 * pjmedia only matches payload types by encoding name and clock rate, which
 * can pair payload types of different profiles when several H.264 payloads
 * are offered. Reject pairings whose profile_idc differ.
 *
 * RFC 6184 §8.1: the level is symmetric too, unless both the offer and the
 * answer signal level-asymmetry-allowed=1. When the offer does not, conform
 * our answer: drop the level asymmetry signaling and do not exceed the
 * offered level.
 */
static pj_status_t
sdpH264FormatMatch(pj_pool_t* /*pool*/,
                   pjmedia_sdp_media* offer,
                   unsigned o_fmt_idx,
                   pjmedia_sdp_media* answer,
                   unsigned a_fmt_idx,
                   unsigned option)
{
    auto fmtpAttr = [](pjmedia_sdp_media* media, unsigned fmtIdx) {
        return pjmedia_sdp_media_find_attr2(media, "fmtp", &media->desc.fmt[fmtIdx]);
    };
    auto attrValue = [](const pjmedia_sdp_attr* attr) -> std::string_view {
        if (attr && attr->value.ptr && attr->value.slen)
            return std::string_view(attr->value.ptr, attr->value.slen);
        return {};
    };
    auto profileIdc = [](std::string_view fmtpParams) -> int {
        if (auto pl = h264::parseProfileLevelId(fmtpParams))
            return pl->profile & 0xff; // strip constraint flags, keep profile_idc
        // RFC 6184: no profile-level-id implies the Baseline profile
        return 0x42;
    };

    const auto offerParams = attrValue(fmtpAttr(offer, o_fmt_idx));
    auto* answerAttr = fmtpAttr(answer, a_fmt_idx);
    if (profileIdc(offerParams) != profileIdc(attrValue(answerAttr)))
        return PJMEDIA_SDP_EFORMATNOTEQUAL;

    if ((option & PJMEDIA_SDP_NEG_FMT_MATCH_ALLOW_MODIFY_ANSWER) && answerAttr
        && not h264::levelAsymmetryAllowed(offerParams)) {
        // The level is symmetric: rewrite the answer in place (same
        // length), as pjmedia_vid_codec_h264_match_sdp does. Only clamp
        // against an explicit offered profile-level-id, to avoid dropping
        // to the implied level 1 with peers omitting fmtp altogether.
        char* p = answerAttr->value.ptr;
        const std::string_view answerParams(p, answerAttr->value.slen);
        static constexpr std::string_view laa = "level-asymmetry-allowed=";
        if (auto pos = answerParams.find(laa); pos != std::string_view::npos && pos + laa.size() < answerParams.size())
            p[pos + laa.size()] = '0';
        const auto offerPl = h264::parseProfileLevelId(offerParams);
        const auto answerPl = h264::parseProfileLevelId(answerParams);
        if (offerPl && answerPl && answerPl->level > offerPl->level) {
            static constexpr std::string_view pli = "profile-level-id=";
            const auto pos = answerParams.find(pli);
            const auto hex = fmt::format("{:02x}", offerPl->level & 0xff);
            p[pos + pli.size() + 4] = hex[0];
            p[pos + pli.size() + 5] = hex[1];
        }
    }
    return PJ_SUCCESS;
}

static void
registerH264FormatMatchCallback()
{
    static std::once_flag once;
    std::call_once(once, [] {
        static constexpr pj_str_t h264Name = sip_utils::CONST_PJ_STR("H264");
        if (pjmedia_sdp_neg_register_fmt_match_cb(const_cast<pj_str_t*>(&h264Name), sdpH264FormatMatch) != PJ_SUCCESS)
            JAMI_ERROR("Failed to register H264 SDP format matching callback");
    });
}

/**
 * RFC 7798 §7.2.2: the H.265 configuration parameters (profile-space,
 * profile-id, tier-flag, interop-constraints) are symmetric per payload
 * type; only level-id may be lowered in the answer. Reject pairings of
 * different profiles and conform the answer level.
 */
static pj_status_t
sdpH265FormatMatch(pj_pool_t* pool,
                   pjmedia_sdp_media* offer,
                   unsigned o_fmt_idx,
                   pjmedia_sdp_media* answer,
                   unsigned a_fmt_idx,
                   unsigned option)
{
    auto fmtpValue = [](pjmedia_sdp_media* media, unsigned fmtIdx) -> std::string_view {
        auto* attr = pjmedia_sdp_media_find_attr2(media, "fmtp", &media->desc.fmt[fmtIdx]);
        if (attr && attr->value.ptr && attr->value.slen)
            return std::string_view(attr->value.ptr, attr->value.slen);
        return {};
    };

    const auto offerInfo = h265::parseFmtp(fmtpValue(offer, o_fmt_idx));
    const auto answerInfo = h265::parseFmtp(fmtpValue(answer, a_fmt_idx));
    if (h265::profileFromFmtp(offerInfo) != h265::profileFromFmtp(answerInfo)
        || offerInfo.tierFlag != answerInfo.tierFlag)
        return PJMEDIA_SDP_EFORMATNOTEQUAL;

    if ((option & PJMEDIA_SDP_NEG_FMT_MATCH_ALLOW_MODIFY_ANSWER) && answerInfo.levelId > offerInfo.levelId) {
        // level-id is decimal and variable length: rebuild the answer
        // fmtp value in the negotiation pool.
        if (auto* attr = pjmedia_sdp_media_find_attr2(answer, "fmtp", &answer->desc.fmt[a_fmt_idx])) {
            auto conformed = h265::setLevelId(std::string_view(attr->value.ptr, attr->value.slen), offerInfo.levelId);
            pj_strdup2(pool, &attr->value, conformed.c_str());
        }
    }
    return PJ_SUCCESS;
}

static void
registerH265FormatMatchCallback()
{
    static std::once_flag once;
    std::call_once(once, [] {
        static constexpr pj_str_t h265Name = sip_utils::CONST_PJ_STR("H265");
        if (pjmedia_sdp_neg_register_fmt_match_cb(const_cast<pj_str_t*>(&h265Name), sdpH265FormatMatch) != PJ_SUCCESS)
            JAMI_ERROR("Failed to register H265 SDP format matching callback");
    });
}
#endif

Sdp::Sdp(const std::string& id)
    : memPool_(nullptr, [](pj_pool_t* pool) { pj_pool_release(pool); })
    , publishedIpAddr_()
    , publishedIpAddrType_()
    , telephoneEventPayload_(101) // same as asterisk
    , sessionName_("Call ID " + id)
{
#ifdef ENABLE_VIDEO
    registerH264FormatMatchCallback();
    registerH265FormatMatchCallback();
#endif
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
    SIPAccount::releasePort(localAudioRtpPort_);
#ifdef ENABLE_VIDEO
    SIPAccount::releasePort(localVideoRtpPort_);
#endif
}

std::shared_ptr<SystemCodecInfo>
Sdp::findCodecBySpec(std::string_view codec, const unsigned clockrate) const
{
    // TODO : only manage a list?
    for (const auto& accountCodec : audio_codec_list_) {
        auto audioCodecInfo = std::static_pointer_cast<SystemAudioCodecInfo>(accountCodec);
        if (audioCodecInfo->name == codec
            and (audioCodecInfo->isPCMG722() ? (clockrate == 8000)
                                             : (audioCodecInfo->audioformat.sample_rate == clockrate)))
            return accountCodec;
    }

    for (const auto& accountCodec : video_codec_list_) {
        if (accountCodec->name == codec)
            return accountCodec;
    }
    return nullptr;
}

std::shared_ptr<SystemCodecInfo>
Sdp::findCodecByPayload(const unsigned payloadType)
{
    // TODO : only manage a list?
    for (const auto& accountCodec : audio_codec_list_) {
        if (accountCodec->payloadType == payloadType)
            return accountCodec;
    }

    for (const auto& accountCodec : video_codec_list_) {
        if (accountCodec->payloadType == payloadType)
            return accountCodec;
    }
    return nullptr;
}

static void
randomFill(std::vector<uint8_t>& dest)
{
    std::uniform_int_distribution<int> rand_byte {0, std::numeric_limits<uint8_t>::max()};
    std::random_device rdev;
    std::generate(dest.begin(), dest.end(), std::bind(rand_byte, std::ref(rdev)));
}

void
Sdp::setActiveLocalSdpSession(const pjmedia_sdp_session* sdp)
{
    if (activeLocalSession_ != sdp)
        JAMI_LOG("Set active local session to [{}]. Was [{}]", fmt::ptr(sdp), fmt::ptr(activeLocalSession_));
    activeLocalSession_ = sdp;
}

void
Sdp::setActiveRemoteSdpSession(const pjmedia_sdp_session* sdp)
{
    if (activeLocalSession_ != sdp)
        JAMI_LOG("Set active remote session to [{}]. Was [{}]", fmt::ptr(sdp), fmt::ptr(activeRemoteSession_));
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

    std::string crypto_attr = "1 "s + jami::CryptoSuites[cryptoSuite].name + " inline:" + base64::encode(keyAndSalt);
    pj_str_t val {sip_utils::CONST_PJ_STR(crypto_attr)};
    return pjmedia_sdp_attr_create(memPool_.get(), "crypto", &val);
}

char const*
Sdp::mediaDirection(const MediaAttribute& mediaAttr)
{
    if (not mediaAttr.enabled_) {
        return DIRECTION_STR[MediaDirection::INACTIVE];
    }

    // Since mute/un-mute audio is only done locally (RTP packets
    // are still sent to the peer), the media direction must be
    // set to "sendrecv" regardless of the mute state.
    if (mediaAttr.type_ == MediaType::MEDIA_AUDIO) {
        return DIRECTION_STR[MediaDirection::SENDRECV];
    }

    if (mediaAttr.muted_) {
        if (mediaAttr.hold_) {
            return DIRECTION_STR[MediaDirection::INACTIVE];
        }
        return DIRECTION_STR[MediaDirection::RECVONLY];
    }

    if (mediaAttr.hold_) {
        return DIRECTION_STR[MediaDirection::SENDONLY];
    }

    return DIRECTION_STR[MediaDirection::SENDRECV];
}

MediaDirection
Sdp::getMediaDirection(pjmedia_sdp_media* media)
{
    if (pjmedia_sdp_attr_find2(media->attr_count, media->attr, DIRECTION_STR[MediaDirection::SENDONLY], nullptr)
        != nullptr) {
        return MediaDirection::SENDONLY;
    }

    if (pjmedia_sdp_attr_find2(media->attr_count, media->attr, DIRECTION_STR[MediaDirection::RECVONLY], nullptr)
        != nullptr) {
        return MediaDirection::RECVONLY;
    }

    if (pjmedia_sdp_attr_find2(media->attr_count, media->attr, DIRECTION_STR[MediaDirection::INACTIVE], nullptr)
        != nullptr) {
        return MediaDirection::INACTIVE;
    }

    // According to RFC 3264 (https://datatracker.ietf.org/doc/html/rfc3264#section-5.1),
    // "a=sendrecv" is the default media direction attribute.
    return MediaDirection::SENDRECV;
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
        auto* const attribute = media->attr[j];
        if (pj_stricmp2(&attribute->name, "crypto") == 0)
            crypto.emplace_back(attribute->value.ptr, attribute->value.slen);
    }

    return crypto;
}

pjmedia_sdp_media*
Sdp::addMediaDescription(const MediaAttribute& mediaAttr)
{
    auto type = mediaAttr.type_;
    auto secure = mediaAttr.secure_;

    JAMI_LOG("Add media description [{}]", mediaAttr.toString(true));

    pjmedia_sdp_media* med = PJ_POOL_ZALLOC_T(memPool_.get(), pjmedia_sdp_media);

#ifdef ENABLE_VIDEO
    // One payload type per (codec, format parameters) pair. For H.264,
    // offer one payload type per encodable and decodable profile
    // (RFC 6184 §8.2.2 requires symmetric profile support per payload
    // type, RFC 3264 §6.1 lets the sender pick any negotiated payload).
    // The preferred payload is listed first: high chroma profiles when
    // sharing a screen, the interop-safe baseline profile otherwise.
    struct VideoPayload
    {
        std::shared_ptr<SystemCodecInfo> codec;
        std::string parameters;
    };
    std::vector<VideoPayload> videoPayloads;
    if (type == MediaType::MEDIA_VIDEO) {
        static const std::string displayPrefix = std::string(libjami::Media::VideoProtocolPrefix::DISPLAY)
                                                 + libjami::Media::VideoProtocolPrefix::SEPARATOR;
        const bool screenShare = mediaAttr.sourceUri_.rfind(displayPrefix, 0) == 0;
        for (const auto& codec : video_codec_list_) {
            if (codec->name == "H264") {
                const auto videoCodec = std::static_pointer_cast<SystemVideoCodecInfo>(codec);
                // RFC 6184 §8.1: advertise support for level asymmetry so both
                // directions can use their own level when the peer supports it.
                auto withLevelAsymmetry = [](std::string params) {
                    if (params.find("level-asymmetry-allowed=") == std::string::npos)
                        params += ";level-asymmetry-allowed=1";
                    return params;
                };
                const std::string baseline = withLevelAsymmetry(videoCodec->parameters.empty()
                                                                    ? libav_utils::DEFAULT_H264_PROFILE_LEVEL_ID
                                                                    : videoCodec->parameters);
                // Advertise high profiles with the same level as the baseline payload
                int level = 0x29;
                if (auto pl = h264::parseProfileLevelId(baseline))
                    level = pl->level;
                std::vector<std::string> highProfiles;
                for (int profile : h264::negotiableHighProfiles())
                    highProfiles.emplace_back(withLevelAsymmetry(h264::makeProfileLevelId(profile, level)));

                if (not screenShare)
                    videoPayloads.emplace_back(VideoPayload {codec, baseline});
                for (auto& params : highProfiles)
                    videoPayloads.emplace_back(VideoPayload {codec, std::move(params)});
                if (screenShare)
                    videoPayloads.emplace_back(VideoPayload {codec, baseline});
            } else if (codec->name == "H265") {
                // RFC 7798 §7.2.2: profile parameters are symmetric per
                // payload type; advertise one payload per profile the local
                // encoders and decoders support. level-id 123 = level 4.1.
                static constexpr int levelId = 123;
                const std::string main = h265::fmtpParams(h265::Profile::Main, levelId);
                auto highProfiles = h265::negotiableHighProfiles();

                if (not screenShare) {
                    // Camera: prefer Main 10 by default when the hardware
                    // supports it (less banding, better compression at
                    // conferencing bitrates), then the interop-safe Main
                    // profile; high chroma payloads are capability adverts.
                    if (auto it = std::find(highProfiles.begin(), highProfiles.end(), h265::Profile::Main10);
                        it != highProfiles.end()) {
                        highProfiles.erase(it);
                        videoPayloads.emplace_back(
                            VideoPayload {codec, h265::fmtpParams(h265::Profile::Main10, levelId)});
                    }
                    videoPayloads.emplace_back(VideoPayload {codec, main});
                    for (auto profile : highProfiles)
                        videoPayloads.emplace_back(VideoPayload {codec, h265::fmtpParams(profile, levelId)});
                } else {
                    // Screen share: highest chroma first, Main last
                    for (auto profile : highProfiles)
                        videoPayloads.emplace_back(VideoPayload {codec, h265::fmtpParams(profile, levelId)});
                    videoPayloads.emplace_back(VideoPayload {codec, main});
                }
            } else {
                videoPayloads.emplace_back(VideoPayload {codec, {}});
            }
        }
    }
#endif

    switch (type) {
    case MediaType::MEDIA_AUDIO:
        med->desc.media = sip_utils::CONST_PJ_STR("audio");
        med->desc.port = mediaAttr.enabled_ ? localAudioRtpPort_ : 0;
        med->desc.fmt_count = audio_codec_list_.size();
        break;
    case MediaType::MEDIA_VIDEO:
        med->desc.media = sip_utils::CONST_PJ_STR("video");
        med->desc.port = mediaAttr.enabled_ ? localVideoRtpPort_ : 0;
#ifdef ENABLE_VIDEO
        med->desc.fmt_count = videoPayloads.size();
#else
        med->desc.fmt_count = video_codec_list_.size();
#endif
        break;
    default:
        throw SdpException("Unsupported media type! Only audio and video are supported");
        break;
    }

    med->desc.port_count = 1;

    // Set the transport protocol of the media
    med->desc.transport = secure ? sip_utils::CONST_PJ_STR("RTP/SAVP") : sip_utils::CONST_PJ_STR("RTP/AVP");

    unsigned dynamic_payload = 96;

    for (unsigned i = 0; i < med->desc.fmt_count; i++) {
        pjmedia_sdp_rtpmap rtpmap;
        rtpmap.param.slen = 0;

        std::string channels; // must have the lifetime of rtpmap
        std::string enc_name;
        unsigned payload;

        if (type == MediaType::MEDIA_AUDIO) {
            auto accountAudioCodec = std::static_pointer_cast<SystemAudioCodecInfo>(audio_codec_list_[i]);
            payload = accountAudioCodec->payloadType;
            enc_name = accountAudioCodec->name;

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
#ifdef ENABLE_VIDEO
            enc_name = videoPayloads[i].codec->name;
#else
            enc_name = video_codec_list_[i]->name;
#endif
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
        if (type == MediaType::MEDIA_VIDEO and not videoPayloads[i].parameters.empty()) {
            // Create a proper fmtp attribute (name "fmtp", value "<pt> <params>")
            // so it can be found back by pjmedia_sdp_media_find_attr, in
            // particular by the H264 format matching callback during negotiation.
            auto value = fmt::format("{} {}", payload, videoPayloads[i].parameters);
            auto pjValue = sip_utils::CONST_PJ_STR(value);
            med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_.get(), "fmtp", &pjValue);
        }
#endif
    }

    if (type == MediaType::MEDIA_AUDIO) {
        setTelephoneEventRtpmap(med);
        if (localAudioRtcpPort_) {
            addRTCPAttribute(med, localAudioRtcpPort_);
        }
    } else if (type == MediaType::MEDIA_VIDEO and localVideoRtcpPort_) {
        addRTCPAttribute(med, localVideoRtcpPort_);
    }

    char const* direction = mediaDirection(mediaAttr);

    med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_.get(), direction, NULL);

    if (secure) {
        if (pjmedia_sdp_media_add_attr(med, generateSdesAttribute()) != PJ_SUCCESS)
            throw SdpException("Unable to add sdes attribute to media");
    }

    return med;
}

void
Sdp::addRTCPAttribute(pjmedia_sdp_media* med, uint16_t port)
{
    dhtnet::IpAddr addr {publishedIpAddr_};
    addr.setPort(port);
    pjmedia_sdp_attr* attr = pjmedia_sdp_attr_create_rtcp(memPool_.get(), addr.pjPtr());
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
            JAMI_ERROR("Unable to validate SDP");
    }
}

void
Sdp::setPublishedIP(const dhtnet::IpAddr& ip_addr)
{
    setPublishedIP(ip_addr, ip_addr.getFamily());
}

void
Sdp::setTelephoneEventRtpmap(pjmedia_sdp_media* med)
{
    ++med->desc.fmt_count;
    pj_strdup2(memPool_.get(), &med->desc.fmt[med->desc.fmt_count - 1], std::to_string(telephoneEventPayload_).c_str());

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
Sdp::setLocalMediaCapabilities(MediaType type, const std::vector<std::shared_ptr<SystemCodecInfo>>& selectedCodecs)
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
                                                   [](const std::shared_ptr<SystemCodecInfo>& i) {
                                                       return i->name == "H265";
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

constexpr std::string_view
Sdp::getSdpDirectionStr(SdpDirection direction)
{
    if (direction == SdpDirection::OFFER)
        return "OFFER"sv;
    if (direction == SdpDirection::ANSWER)
        return "ANSWER"sv;
    return "NONE"sv;
}

void
Sdp::printSession(const pjmedia_sdp_session* session, const char* header, SdpDirection direction)
{
    static constexpr size_t BUF_SZ = 4095;
    sip_utils::PoolPtr tmpPool_(pj_pool_create(&Manager::instance().sipVoIPLink().getCachingPool()->factory,
                                               "printSdp",
                                               BUF_SZ,
                                               BUF_SZ,
                                               nullptr));

    auto* cloned_session = pjmedia_sdp_session_clone(tmpPool_.get(), session);
    if (!cloned_session) {
        JAMI_ERROR("Unable to clone SDP for printing");
        return;
    }

    // Filter-out sensible data like SRTP master key.
    for (unsigned i = 0; i < cloned_session->media_count; ++i) {
        pjmedia_sdp_media_remove_all_attr(cloned_session->media[i], "crypto");
    }

    std::array<char, BUF_SZ + 1> buffer;
    auto size = pjmedia_sdp_print(cloned_session, buffer.data(), BUF_SZ);
    if (size < 0) {
        JAMI_ERROR("SDP too big for dump: {}", header);
        return;
    }

    JAMI_LOG("[SDP {}] {}\n{:s}", getSdpDirectionStr(direction), header, std::string_view(buffer.data(), size));
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
    JAMI_DEBUG("Creating SDP offer with {} media", mediaList.size());

    createLocalSession(SdpDirection::OFFER);

    if (validateSession() != PJ_SUCCESS) {
        JAMI_ERROR("Failed to create initial offer");
        return false;
    }

    localSession_->media_count = 0;

    for (auto const& media : mediaList) {
        if (media.enabled_) {
            localSession_->media[localSession_->media_count++] = addMediaDescription(media);
        }
    }

    if (validateSession() != PJ_SUCCESS) {
        JAMI_ERROR("Failed to add medias");
        return false;
    }

    if (pjmedia_sdp_neg_create_w_local_offer(memPool_.get(), localSession_, &negotiator_) != PJ_SUCCESS) {
        JAMI_ERROR("Failed to create an initial SDP negotiator");
        return false;
    }

    printSession(localSession_, "Local session (initial):", sdpDirection_);

    return true;
}

void
Sdp::setReceivedOffer(const pjmedia_sdp_session* remote)
{
    if (remote == nullptr) {
        JAMI_ERROR("Remote session is NULL");
        return;
    }
    remoteSession_ = pjmedia_sdp_session_clone(memPool_.get(), remote);
}

bool
Sdp::processIncomingOffer(const std::vector<MediaAttribute>& mediaList)
{
    if (not remoteSession_)
        return false;

    JAMI_DEBUG("Processing received offer for [{:s}] with {:d} media", sessionName_, mediaList.size());

    printSession(remoteSession_, "Remote session:", SdpDirection::OFFER);

    createLocalSession(SdpDirection::ANSWER);
    if (validateSession() != PJ_SUCCESS) {
        JAMI_ERROR("Failed to create local session");
        return false;
    }

    localSession_->media_count = 0;

    for (auto const& media : mediaList) {
        if (media.enabled_) {
            localSession_->media[localSession_->media_count++] = addMediaDescription(media);
        }
    }

    printSession(localSession_, "Local session:\n", sdpDirection_);

    if (validateSession() != PJ_SUCCESS) {
        JAMI_ERROR("Failed to add medias");
        return false;
    }

    if (pjmedia_sdp_neg_create_w_remote_offer(memPool_.get(), localSession_, remoteSession_, &negotiator_)
        != PJ_SUCCESS) {
        JAMI_ERROR("Failed to initialize media negotiation");
        return false;
    }

    return true;
}

bool
Sdp::startNegotiation()
{
    JAMI_LOG("Starting media negotiation for [{}]", sessionName_);

    if (negotiator_ == NULL) {
        JAMI_ERROR("Unable to start negotiation with invalid negotiator");
        return false;
    }

    const pjmedia_sdp_session* active_local;
    const pjmedia_sdp_session* active_remote;

    if (pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO) {
        JAMI_WARNING("Negotiator not in right state for negotiation");
        return false;
    }

    if (pjmedia_sdp_neg_negotiate(memPool_.get(), negotiator_, 0) != PJ_SUCCESS) {
        JAMI_ERROR("Failed to start media negotiation");
        return false;
    }

    if (pjmedia_sdp_neg_get_active_local(negotiator_, &active_local) != PJ_SUCCESS)
        JAMI_ERROR("Unable to retrieve local active session");

    setActiveLocalSdpSession(active_local);

    if (active_local != nullptr) {
        printSession(active_local, "Local active session:", sdpDirection_);
    }

    if (pjmedia_sdp_neg_get_active_remote(negotiator_, &active_remote) != PJ_SUCCESS or active_remote == nullptr) {
        JAMI_ERROR("Unable to retrieve remote active session");
        return false;
    }

    setActiveRemoteSdpSession(active_remote);

    printSession(active_remote, "Remote active session:", sdpDirection_);

    return true;
}

std::string
Sdp::getFilteredSdp(const pjmedia_sdp_session* session, unsigned media_keep, unsigned pt_keep)
{
    static constexpr size_t BUF_SZ = 4096;
    sip_utils::PoolPtr tmpPool_(
        pj_pool_create(&Manager::instance().sipVoIPLink().getCachingPool()->factory, "tmpSdp", BUF_SZ, BUF_SZ, nullptr));
    auto* cloned = pjmedia_sdp_session_clone(tmpPool_.get(), session);
    if (!cloned) {
        JAMI_ERROR("Unable to clone SDP");
        return "";
    }

    // deactivate non-video media
    bool hasKeep = false;
    for (unsigned i = 0; i < cloned->media_count; i++)
        if (i != media_keep) {
            if (pjmedia_sdp_media_deactivate(tmpPool_.get(), cloned->media[i]) != PJ_SUCCESS)
                JAMI_ERROR("Unable to deactivate media");
        } else {
            hasKeep = true;
        }

    if (not hasKeep) {
        JAMI_LOG("No media to keep present in SDP");
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
        auto* media = cloned->media[i];

        // filter other codecs
        for (unsigned c = 0; c < media->desc.fmt_count; c++) {
            auto& pt = media->desc.fmt[c];
            if (pj_strtoul(&pt) == pt_keep)
                continue;

            while (auto* attr = pjmedia_sdp_attr_find2(media->attr_count, media->attr, "rtpmap", &pt))
                pjmedia_sdp_attr_remove(&media->attr_count, media->attr, attr);

            while (auto* attr = pjmedia_sdp_attr_find2(media->attr_count, media->attr, "fmt", &pt))
                pjmedia_sdp_attr_remove(&media->attr_count, media->attr, attr);

            std::move(media->desc.fmt + c + 1, media->desc.fmt + media->desc.fmt_count, media->desc.fmt + c);
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
        auto* media = session->media[i];
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
            JAMI_ERROR("Unable to find connection information for media");
            continue;
        }
        descr.addr = std::string_view(conn->addr.ptr, conn->addr.slen);
        descr.addr.setPort(media->desc.port);

        // Get the "rtcp" address from the SDP if present. Otherwise,
        // infere it from endpoint (RTP) address.
        auto* attr = pjmedia_sdp_attr_find2(media->attr_count, media->attr, "rtcp", NULL);
        if (attr) {
            pjmedia_sdp_rtcp_attr rtcp;
            auto status = pjmedia_sdp_attr_get_rtcp(attr, &rtcp);
            if (status == PJ_SUCCESS && rtcp.addr.slen) {
                descr.rtcp_addr = std::string_view(rtcp.addr.ptr, rtcp.addr.slen);
                descr.rtcp_addr.setPort(rtcp.port);
            }
        }

        descr.hold = pjmedia_sdp_attr_find2(media->attr_count,
                                            media->attr,
                                            DIRECTION_STR[MediaDirection::SENDONLY],
                                            nullptr)
                     || pjmedia_sdp_attr_find2(media->attr_count,
                                               media->attr,
                                               DIRECTION_STR[MediaDirection::INACTIVE],
                                               nullptr);

        descr.direction_ = getMediaDirection(media);

        // get codecs infos
        for (unsigned j = 0; j < media->desc.fmt_count; j++) {
            auto* const rtpMapAttribute = pjmedia_sdp_media_find_attr(media, &STR_RTPMAP, &media->desc.fmt[j]);
            if (!rtpMapAttribute) {
                JAMI_ERROR("Unable to find rtpmap attribute");
                descr.enabled = false;
                continue;
            }
            pjmedia_sdp_rtpmap rtpmap;
            if (pjmedia_sdp_attr_get_rtpmap(rtpMapAttribute, &rtpmap) != PJ_SUCCESS || rtpmap.enc_name.slen == 0) {
                JAMI_ERROR("Unable to find payload type {} in SDP", sip_utils::as_view(media->desc.fmt[j]));
                descr.enabled = false;
                continue;
            }
            auto codec_raw = sip_utils::as_view(rtpmap.enc_name);
            descr.rtp_clockrate = rtpmap.clock_rate;
            descr.codec = findCodecBySpec(codec_raw, rtpmap.clock_rate);
            if (not descr.codec) {
                JAMI_ERROR("Unable to find codec {}", codec_raw);
                descr.enabled = false;
                continue;
            }
            descr.payload_type = pj_strtoul(&rtpmap.pt);
            if (descr.type == MEDIA_VIDEO) {
                auto* const fmtpAttr = pjmedia_sdp_media_find_attr(media, &STR_FMTP, &media->desc.fmt[j]);
                // descr.bitrate = getOutgoingVideoField(codec, "bitrate");
                descr.parameters.clear();
                if (fmtpAttr && fmtpAttr->value.ptr && fmtpAttr->value.slen) {
                    const auto& v = fmtpAttr->value;
                    descr.parameters = std::string(v.ptr, v.ptr + v.slen);
                }
#ifdef ENABLE_VIDEO
                if (remote and activeLocalSession_ and i < activeLocalSession_->media_count
                    and (descr.codec->name == "H264" or descr.codec->name == "H265")) {
                    // The active remote session may list several payload
                    // types per codec (one per profile); the negotiated
                    // one is the payload whose profile matches our own
                    // active payload (RFC 6184 §8.2.2, RFC 7798 §7.2.2).
                    // Its fmtp drives our encoder: clamp the level to the
                    // negotiated (symmetric) level when applicable.
                    auto* localMedia = activeLocalSession_->media[i];
                    std::string_view localParams;
                    if (localMedia->desc.fmt_count > 0) {
                        if (auto* localFmtp = pjmedia_sdp_media_find_attr(localMedia,
                                                                          &STR_FMTP,
                                                                          &localMedia->desc.fmt[0])) {
                            if (localFmtp->value.ptr && localFmtp->value.slen)
                                localParams = std::string_view(localFmtp->value.ptr, localFmtp->value.slen);
                        }
                    }
                    if (descr.codec->name == "H264") {
                        auto profileIdc = [](std::string_view params) -> int {
                            if (auto pl = h264::parseProfileLevelId(params))
                                return pl->profile & 0xff;
                            return 0x42; // RFC 6184: default Baseline
                        };
                        if (profileIdc(localParams) != profileIdc(descr.parameters))
                            continue; // profile mismatch: not the negotiated payload
                        // RFC 6184 §8.1: level asymmetry requires
                        // level-asymmetry-allowed=1 in both the offer and
                        // the answer; otherwise the level is symmetric,
                        // i.e. the lower of both advertised levels.
                        if (not(h264::levelAsymmetryAllowed(localParams)
                                and h264::levelAsymmetryAllowed(descr.parameters))) {
                            const auto localPl = h264::parseProfileLevelId(localParams);
                            const auto remotePl = h264::parseProfileLevelId(descr.parameters);
                            if (localPl && remotePl && localPl->level < remotePl->level)
                                descr.parameters = h264::setLevel(descr.parameters, localPl->level);
                        }
                    } else {
                        const auto localInfo = h265::parseFmtp(localParams);
                        const auto remoteInfo = h265::parseFmtp(descr.parameters);
                        if (h265::profileFromFmtp(localInfo) != h265::profileFromFmtp(remoteInfo))
                            continue; // profile mismatch: not the negotiated payload
                        // RFC 7798 §7.2.2: level-id is symmetric up to the
                        // lower of both advertised levels.
                        if (localInfo.levelId < remoteInfo.levelId)
                            descr.parameters = h265::setLevelId(descr.parameters, localInfo.levelId);
                    }
                }
#endif
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
            auto* const attribute = media->attr[j];
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
        JAMI_ERROR("addIceCandidates failed: unable to access media#{} (may be deactivated)", media_index);
        return;
    }

    auto* media = localSession_->media[media_index];

    for (const auto& item : cands) {
        const pj_str_t val = sip_utils::CONST_PJ_STR(item);
        pjmedia_sdp_attr* attr = pjmedia_sdp_attr_create(memPool_.get(), "candidate", &val);

        if (pjmedia_sdp_media_add_attr(media, attr) != PJ_SUCCESS)
            throw SdpException("Unable to add ICE candidates attribute to media");
    }
}

std::vector<std::string>
Sdp::getIceCandidates(unsigned media_index) const
{
    const auto* remoteSession = activeRemoteSession_ ? activeRemoteSession_ : remoteSession_;
    const auto* localSession = activeLocalSession_ ? activeLocalSession_ : localSession_;
    if (not remoteSession) {
        JAMI_ERROR("getIceCandidates failed: no remote session");
        return {};
    }
    if (not localSession) {
        JAMI_ERROR("getIceCandidates failed: no local session");
        return {};
    }
    if (media_index >= remoteSession->media_count || media_index >= localSession->media_count) {
        JAMI_ERROR("getIceCandidates failed: unable to access media#{} (may be deactivated)", media_index);
        return {};
    }
    auto* media = remoteSession->media[media_index];
    auto* localMedia = localSession->media[media_index];
    if (media->desc.port == 0 || localMedia->desc.port == 0) {
        JAMI_WARNING("Media#{} is disabled. Media ports: local {}, remote {}",
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
Sdp::addIceAttributes(const dhtnet::IceTransport::Attribute&& ice_attrs)
{
    pj_str_t value = sip_utils::CONST_PJ_STR(ice_attrs.ufrag);
    pjmedia_sdp_attr* attr = pjmedia_sdp_attr_create(memPool_.get(), "ice-ufrag", &value);

    if (pjmedia_sdp_attr_add(&localSession_->attr_count, localSession_->attr, attr) != PJ_SUCCESS)
        throw SdpException("Unable to add ICE.ufrag attribute to local SDP");

    value = sip_utils::CONST_PJ_STR(ice_attrs.pwd);
    attr = pjmedia_sdp_attr_create(memPool_.get(), "ice-pwd", &value);

    if (pjmedia_sdp_attr_add(&localSession_->attr_count, localSession_->attr, attr) != PJ_SUCCESS)
        throw SdpException("Unable to add ICE.pwd attribute to local SDP");
}

dhtnet::IceTransport::Attribute
Sdp::getIceAttributes() const
{
    if (const auto* session = activeRemoteSession_ ? activeRemoteSession_ : remoteSession_)
        return getIceAttributes(session);
    return {};
}

dhtnet::IceTransport::Attribute
Sdp::getIceAttributes(const pjmedia_sdp_session* session)
{
    dhtnet::IceTransport::Attribute ice_attrs;
    // Per RFC8839, ice-ufrag/ice-pwd can be present either at
    // media or session level.
    // This seems to be the case for Asterisk servers (ICE is at media-session).
    for (unsigned i = 0; i < session->attr_count; i++) {
        pjmedia_sdp_attr* attribute = session->attr[i];
        if (pj_stricmp2(&attribute->name, "ice-ufrag") == 0)
            ice_attrs.ufrag.assign(attribute->value.ptr, attribute->value.slen);
        else if (pj_stricmp2(&attribute->name, "ice-pwd") == 0)
            ice_attrs.pwd.assign(attribute->value.ptr, attribute->value.slen);
        if (!ice_attrs.ufrag.empty() && !ice_attrs.pwd.empty())
            return ice_attrs;
    }
    for (unsigned i = 0; i < session->media_count; i++) {
        auto* media = session->media[i];
        for (unsigned j = 0; j < media->attr_count; j++) {
            pjmedia_sdp_attr* attribute = media->attr[j];
            if (pj_stricmp2(&attribute->name, "ice-ufrag") == 0)
                ice_attrs.ufrag.assign(attribute->value.ptr, attribute->value.slen);
            else if (pj_stricmp2(&attribute->name, "ice-pwd") == 0)
                ice_attrs.pwd.assign(attribute->value.ptr, attribute->value.slen);
            if (!ice_attrs.ufrag.empty() && !ice_attrs.pwd.empty())
                return ice_attrs;
        }
    }

    return ice_attrs;
}

void
Sdp::clearIce()
{
    clearIce(localSession_);
    clearIce(remoteSession_);
    setActiveRemoteSdpSession(nullptr);
    setActiveLocalSdpSession(nullptr);
}

void
Sdp::clearIce(pjmedia_sdp_session* session)
{
    if (not session)
        return;
    pjmedia_sdp_attr_remove_all(&session->attr_count, session->attr, "ice-ufrag");
    pjmedia_sdp_attr_remove_all(&session->attr_count, session->attr, "ice-pwd");
    // TODO. Why this? we should not have "candidate" attribute at session level.
    pjmedia_sdp_attr_remove_all(&session->attr_count, session->attr, "candidate");
    for (unsigned i = 0; i < session->media_count; i++) {
        auto* media = session->media[i];
        pjmedia_sdp_attr_remove_all(&media->attr_count, media->attr, "candidate");
    }
}

std::vector<MediaAttribute>
Sdp::getMediaAttributeListFromSdp(const pjmedia_sdp_session* sdpSession, bool ignoreDisabled)
{
    if (sdpSession == nullptr) {
        return {};
    }

    std::vector<MediaAttribute> mediaList;
    unsigned audioIdx = 0;
    unsigned videoIdx = 0;
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
            JAMI_WARNING("Media#{} only 'audio' and 'video' types are supported!", idx);
            // Disable the media. No need to parse the attributes.
            mediaAttr.enabled_ = false;
            continue;
        }

        // Set enabled flag
        mediaAttr.enabled_ = media->desc.port > 0;

        if (!mediaAttr.enabled_ && ignoreDisabled) {
            mediaList.pop_back();
            continue;
        }

        // Get mute state.
        auto direction = getMediaDirection(media);
        mediaAttr.muted_ = direction != MediaDirection::SENDRECV and direction != MediaDirection::SENDONLY;

        // Get transport.
        auto transp = getMediaTransport(media);
        if (transp == MediaTransport::UNKNOWN) {
            JAMI_WARNING("Media#{} is unable to determine transport type!", idx);
        }

        // A media is secure if the transport is of type RTP/SAVP
        // and the crypto materials are present.
        mediaAttr.secure_ = transp == MediaTransport::RTP_SAVP and not getCrypto(media).empty();

        if (mediaAttr.type_ == MediaType::MEDIA_AUDIO) {
            mediaAttr.label_ = "audio_" + std::to_string(audioIdx++);
        } else if (mediaAttr.type_ == MediaType::MEDIA_VIDEO) {
            mediaAttr.label_ = "video_" + std::to_string(videoIdx++);
        }
    }

    return mediaList;
}

} // namespace jami
