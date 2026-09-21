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

#include "media_codec.h"
#include "sdes_negotiator.h"

#include <opendht/rng.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <optional>

namespace jami {

using std::string;
using std::vector;

static constexpr int POOL_INITIAL_SIZE = 16384;
static constexpr int POOL_INCREMENT_SIZE = POOL_INITIAL_SIZE;

static std::map<MediaDirection, const char*> DIRECTION_STR {{MediaDirection::SENDRECV, "sendrecv"},
                                                            {MediaDirection::SENDONLY, "sendonly"},
                                                            {MediaDirection::RECVONLY, "recvonly"},
                                                            {MediaDirection::INACTIVE, "inactive"}};

static bool
hasRtcpMuxAttribute(const pjmedia_sdp_media* media)
{
    return media and pjmedia_sdp_attr_find2(media->attr_count, media->attr, "rtcp-mux", nullptr) != nullptr;
}

static std::optional<unsigned>
findOfferedPayloadType(const pjmedia_sdp_media* media, std::string_view codecName, unsigned clockRate)
{
    static constexpr pj_str_t STR_RTPMAP {sip_utils::CONST_PJ_STR("rtpmap")};

    if (not media)
        return std::nullopt;

    for (unsigned i = 0; i < media->desc.fmt_count; ++i) {
        auto* attribute = pjmedia_sdp_media_find_attr(media, &STR_RTPMAP, &media->desc.fmt[i]);
        if (not attribute)
            continue;

        pjmedia_sdp_rtpmap rtpmap {};
        if (pjmedia_sdp_attr_get_rtpmap(attribute, &rtpmap) != PJ_SUCCESS)
            continue;

        const auto offeredName = sip_utils::as_view(rtpmap.enc_name);
        const auto sameName = offeredName.size() == codecName.size()
                              and std::equal(offeredName.begin(),
                                             offeredName.end(),
                                             codecName.begin(),
                                             [](char a, char b) {
                                                 return std::tolower(static_cast<unsigned char>(a))
                                                        == std::tolower(static_cast<unsigned char>(b));
                                             });
        if (sameName and rtpmap.clock_rate == clockRate)
            return pj_strtoul(&rtpmap.pt);
    }

    return std::nullopt;
}

Sdp::Sdp(const std::string& id)
    : memPool_(nullptr, [](pj_pool_t* pool) { pj_pool_release(pool); })
    , publishedIpAddr_()
    , publishedIpAddrType_()
    , telephoneEventPayload_(101) // same as asterisk
    , sessionName_("Call ID " + id)
{
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
    // Codec names are case-insensitive (RFC 4566 6.).
    const auto sameName = [&codec](std::string_view name) {
        return name.size() == codec.size() and std::equal(name.begin(), name.end(), codec.begin(), [](char a, char b) {
                   return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
               });
    };

    // TODO : only manage a list?
    for (const auto& accountCodec : audio_codec_list_) {
        auto audioCodecInfo = std::static_pointer_cast<SystemAudioCodecInfo>(accountCodec);
        if (sameName(audioCodecInfo->name)
            and (audioCodecInfo->isPCMG722() ? (clockrate == 8000)
                                             : (audioCodecInfo->audioformat.sample_rate == clockrate)))
            return accountCodec;
    }

    for (const auto& accountCodec : video_codec_list_) {
        if (sameName(accountCodec->name))
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

namespace {

constexpr std::array<std::string_view, 2> SDES_OFFER_CRYPTO_SUITES {
    "AES_256_CM_HMAC_SHA1_80",
    "AES_CM_128_HMAC_SHA1_80",
};

constexpr std::string_view MID_RTP_EXTENSION_URI {"urn:ietf:params:rtp-hdrext:sdes:mid"};
constexpr unsigned DEFAULT_MID_RTP_EXTENSION_ID {1};
constexpr unsigned DEFAULT_TRANSPORT_CC_RTP_EXTENSION_ID {3};
constexpr std::array<std::string_view, 3> TRANSPORT_CC_RTP_EXTENSION_URIS {
    "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01",
    "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-02",
    "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-03",
};
constexpr auto TRANSPORT_CC_RTP_EXTENSION_URI = TRANSPORT_CC_RTP_EXTENSION_URIS.front();

const CryptoSuiteDefinition*
findCryptoSuiteDefinition(std::string_view cryptoSuite)
{
    const auto it = std::find_if(CryptoSuites.begin(), CryptoSuites.end(), [cryptoSuite](const auto& suite) {
        return suite.name == cryptoSuite;
    });
    return it != CryptoSuites.end() ? &*it : nullptr;
}

bool
isDtlsTransport(MediaTransport transport)
{
    return transport == MediaTransport::UDP_TLS_RTP_SAVP || transport == MediaTransport::UDP_TLS_RTP_SAVPF;
}

DtlsSetup
parseDtlsSetup(std::string_view value)
{
    if (value == "actpass")
        return DtlsSetup::ACTPASS;
    if (value == "active")
        return DtlsSetup::ACTIVE;
    if (value == "passive")
        return DtlsSetup::PASSIVE;
    return DtlsSetup::NONE;
}

const char*
dtlsSetupName(DtlsSetup setup)
{
    switch (setup) {
    case DtlsSetup::ACTPASS:
        return "actpass";
    case DtlsSetup::ACTIVE:
        return "active";
    case DtlsSetup::PASSIVE:
        return "passive";
    default:
        return nullptr;
    }
}

DtlsSetup
negotiateLocalDtlsSetup(DtlsSetup localSetup, DtlsSetup remoteSetup)
{
    if (localSetup != DtlsSetup::ACTPASS)
        return localSetup;
    if (remoteSetup == DtlsSetup::ACTIVE)
        return DtlsSetup::PASSIVE;
    if (remoteSetup == DtlsSetup::PASSIVE || remoteSetup == DtlsSetup::ACTPASS || remoteSetup == DtlsSetup::NONE)
        return DtlsSetup::ACTIVE;
    return DtlsSetup::NONE;
}

std::pair<std::string, std::string>
getDtlsFingerprint(const pjmedia_sdp_session* session, unsigned mediaIndex)
{
    if (not session)
        return {};

    const auto parseFingerprint = [](const pjmedia_sdp_attr* attribute) {
        if (not attribute || pj_stricmp2(&attribute->name, "fingerprint") != 0)
            return std::pair<std::string, std::string> {};

        const std::string value(attribute->value.ptr, attribute->value.slen);
        const auto separator = value.find(' ');
        if (separator == std::string::npos)
            return std::pair<std::string, std::string> {};

        return std::pair<std::string, std::string> {value.substr(0, separator), value.substr(separator + 1)};
    };

    if (mediaIndex < session->media_count) {
        auto* media = session->media[mediaIndex];
        for (unsigned i = 0; i < media->attr_count; ++i) {
            if (auto parsed = parseFingerprint(media->attr[i]); not parsed.second.empty())
                return parsed;
        }
    }

    for (unsigned i = 0; i < session->attr_count; ++i) {
        if (auto parsed = parseFingerprint(session->attr[i]); not parsed.second.empty())
            return parsed;
    }

    return {};
}

DtlsSetup
getDtlsSetup(const pjmedia_sdp_session* session, unsigned mediaIndex)
{
    if (not session)
        return DtlsSetup::NONE;

    const auto parseSetupAttribute = [](const pjmedia_sdp_attr* attribute) {
        if (not attribute || pj_stricmp2(&attribute->name, "setup") != 0)
            return DtlsSetup::NONE;
        return parseDtlsSetup(std::string_view(attribute->value.ptr, attribute->value.slen));
    };

    if (mediaIndex < session->media_count) {
        auto* media = session->media[mediaIndex];
        for (unsigned i = 0; i < media->attr_count; ++i) {
            if (const auto setup = parseSetupAttribute(media->attr[i]); setup != DtlsSetup::NONE)
                return setup;
        }
    }

    for (unsigned i = 0; i < session->attr_count; ++i) {
        if (const auto setup = parseSetupAttribute(session->attr[i]); setup != DtlsSetup::NONE)
            return setup;
    }

    return DtlsSetup::NONE;
}

std::string
getMidValue(const pjmedia_sdp_media* media)
{
    if (not media)
        return {};

    if (auto* midAttr = pjmedia_sdp_attr_find2(media->attr_count, media->attr, "mid", nullptr); midAttr)
        return {midAttr->value.ptr, static_cast<size_t>(midAttr->value.slen)};

    return {};
}

bool
hasBundleGroup(const pjmedia_sdp_session* session)
{
    if (not session)
        return false;

    for (unsigned i = 0; i < session->attr_count; ++i) {
        auto* attr = session->attr[i];
        if (not attr || pj_stricmp2(&attr->name, "group") != 0)
            continue;

        const std::string_view value(attr->value.ptr, static_cast<size_t>(attr->value.slen));
        if (value.rfind("BUNDLE ", 0) == 0)
            return true;
    }

    return false;
}

unsigned
findExtmapId(unsigned attrCount, pjmedia_sdp_attr* const* attrs, std::string_view uri)
{
    for (unsigned i = 0; i < attrCount; ++i) {
        auto* attr = attrs[i];
        if (not attr || pj_stricmp2(&attr->name, "extmap") != 0)
            continue;

        const std::string_view value(attr->value.ptr, static_cast<size_t>(attr->value.slen));
        const auto separator = value.find(' ');
        if (separator == std::string_view::npos)
            continue;

        const auto idToken = value.substr(0, separator);
        const auto slash = idToken.find('/');
        const auto numericId = idToken.substr(0, slash);

        unsigned extId = 0;
        for (const auto ch : numericId) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                extId = 0;
                break;
            }
            extId = (extId * 10) + static_cast<unsigned>(ch - '0');
        }

        if (extId == 0)
            continue;

        const auto mappedUri = value.substr(separator + 1);
        const auto uriSeparator = mappedUri.find(' ');
        if (mappedUri.substr(0, uriSeparator) == uri)
            return extId;
    }

    return 0;
}

unsigned
getExtmapId(const pjmedia_sdp_session* session, unsigned mediaIndex, std::string_view uri)
{
    if (not session)
        return 0;

    if (mediaIndex < session->media_count) {
        if (const auto extId = findExtmapId(session->media[mediaIndex]->attr_count,
                                            session->media[mediaIndex]->attr,
                                            uri)) {
            return extId;
        }
    }

    return findExtmapId(session->attr_count, session->attr, uri);
}

unsigned
getExtmapId(const pjmedia_sdp_session* session, unsigned mediaIndex, const std::array<std::string_view, 3>& uris)
{
    for (const auto uri : uris) {
        if (const auto extId = getExtmapId(session, mediaIndex, uri))
            return extId;
    }

    return 0;
}

unsigned
getBundleExtmapId(const pjmedia_sdp_session* session, std::string_view uri)
{
    if (not session)
        return 0;

    unsigned bundleExtId = 0;
    for (unsigned i = 0; i < session->media_count; ++i) {
        const auto extId = getExtmapId(session, i, uri);
        if (extId == 0)
            continue;

        if (bundleExtId == 0) {
            bundleExtId = extId;
            continue;
        }

        if (bundleExtId != extId) {
            JAMI_WARNING("Ignoring inconsistent extmap id {} for {} in BUNDLE group, using {}", extId, uri, bundleExtId);
            return bundleExtId;
        }
    }

    return bundleExtId;
}

unsigned
getBundleExtmapId(const pjmedia_sdp_session* session,
                  const std::array<std::string_view, 3>& uris,
                  std::string_view label)
{
    if (not session)
        return 0;

    unsigned bundleExtId = 0;
    for (unsigned mediaIndex = 0; mediaIndex < session->media_count; ++mediaIndex) {
        const auto extId = getExtmapId(session, mediaIndex, uris);
        if (extId == 0)
            continue;

        if (bundleExtId == 0) {
            bundleExtId = extId;
            continue;
        }

        if (bundleExtId != extId) {
            JAMI_WARNING("Ignoring inconsistent extmap id {} for {} in BUNDLE group, using {}",
                         extId,
                         label,
                         bundleExtId);
            return bundleExtId;
        }
    }

    return bundleExtId;
}

bool
parsePayloadTypeToken(std::string_view token, unsigned payloadType)
{
    if (token == "*")
        return true;
    if (token.empty())
        return false;

    unsigned parsedPayloadType = 0;
    for (const auto character : token) {
        if (!std::isdigit(static_cast<unsigned char>(character)))
            return false;
        parsedPayloadType = (parsedPayloadType * 10) + static_cast<unsigned>(character - '0');
    }

    return parsedPayloadType == payloadType;
}

bool
findRtcpFeedback(unsigned attrCount, pjmedia_sdp_attr* const* attrs, unsigned payloadType, std::string_view feedbackType)
{
    for (unsigned attrIndex = 0; attrIndex < attrCount; ++attrIndex) {
        auto* attr = attrs[attrIndex];
        if (not attr || pj_stricmp2(&attr->name, "rtcp-fb") != 0)
            continue;

        const std::string_view value(attr->value.ptr, static_cast<size_t>(attr->value.slen));
        const auto separator = value.find(' ');
        if (separator == std::string_view::npos)
            continue;

        if (!parsePayloadTypeToken(value.substr(0, separator), payloadType))
            continue;

        const auto feedbackStart = value.find_first_not_of(' ', separator + 1);
        if (feedbackStart == std::string_view::npos)
            continue;

        // Match the full feedback value (e.g. "nack pli", "ccm fir") when the
        // requested type contains a parameter, otherwise match the first token
        // only (e.g. "transport-cc", plain "nack").
        const auto feedback = value.substr(feedbackStart);
        if (feedbackType.find(' ') != std::string_view::npos) {
            if (feedback == feedbackType)
                return true;
            continue;
        }
        const auto feedbackSeparator = feedback.find(' ');
        if (feedback.substr(0, feedbackSeparator) == feedbackType)
            return true;
    }

    return false;
}

bool
findExactRtcpFeedback(unsigned attrCount,
                      pjmedia_sdp_attr* const* attrs,
                      unsigned payloadType,
                      std::string_view feedbackType)
{
    for (unsigned attrIndex = 0; attrIndex < attrCount; ++attrIndex) {
        auto* attr = attrs[attrIndex];
        if (not attr || pj_stricmp2(&attr->name, "rtcp-fb") != 0)
            continue;

        const std::string_view value(attr->value.ptr, static_cast<size_t>(attr->value.slen));
        const auto separator = value.find(' ');
        if (separator == std::string_view::npos
            || !parsePayloadTypeToken(value.substr(0, separator), payloadType)) {
            continue;
        }

        const auto feedbackStart = value.find_first_not_of(' ', separator + 1);
        if (feedbackStart != std::string_view::npos && value.substr(feedbackStart) == feedbackType)
            return true;
    }

    return false;
}

bool
hasRtcpFeedback(const pjmedia_sdp_session* session,
                unsigned mediaIndex,
                unsigned payloadType,
                std::string_view feedbackType)
{
    if (not session)
        return false;

    if (mediaIndex < session->media_count) {
        if (findRtcpFeedback(session->media[mediaIndex]->attr_count,
                             session->media[mediaIndex]->attr,
                             payloadType,
                             feedbackType)) {
            return true;
        }
    }

    return findRtcpFeedback(session->attr_count, session->attr, payloadType, feedbackType);
}

bool
hasAnyRtcpFeedback(const pjmedia_sdp_session* session, unsigned mediaIndex, std::string_view feedbackType)
{
    if (not session || mediaIndex >= session->media_count)
        return false;

    auto* media = session->media[mediaIndex];
    for (unsigned fmtIndex = 0; fmtIndex < media->desc.fmt_count; ++fmtIndex) {
        const auto payloadType = pj_strtoul(&media->desc.fmt[fmtIndex]);
        if (hasRtcpFeedback(session, mediaIndex, payloadType, feedbackType))
            return true;
    }

    return false;
}

bool
hasAnyExactRtcpFeedback(const pjmedia_sdp_session* session,
                        unsigned mediaIndex,
                        std::string_view feedbackType)
{
    if (not session || mediaIndex >= session->media_count)
        return false;

    auto* media = session->media[mediaIndex];
    for (unsigned fmtIndex = 0; fmtIndex < media->desc.fmt_count; ++fmtIndex) {
        const auto payloadType = pj_strtoul(&media->desc.fmt[fmtIndex]);
        if (findExactRtcpFeedback(media->attr_count, media->attr, payloadType, feedbackType)
            || findExactRtcpFeedback(session->attr_count,
                                     session->attr,
                                     payloadType,
                                     feedbackType)) {
            return true;
        }
    }

    return false;
}

} // namespace

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
Sdp::generateSdesAttribute(std::string_view tag, std::string_view cryptoSuite)
{
    const auto* suite = findCryptoSuiteDefinition(cryptoSuite);
    if (not suite)
        throw SdpException("Unsupported SRTP crypto suite");

    std::vector<uint8_t> keyAndSalt;
    keyAndSalt.resize(suite->masterKeyLength / 8 + suite->masterSaltLength / 8);
    randomFill(keyAndSalt);

    std::string crypto_attr = std::string(tag) + " " + std::string(cryptoSuite)
                              + " inline:" + base64::encode(keyAndSalt);
    pj_str_t val {sip_utils::CONST_PJ_STR(crypto_attr)};
    return pjmedia_sdp_attr_create(memPool_.get(), "crypto", &val);
}

std::vector<pjmedia_sdp_attr*>
Sdp::generateSdesOfferAttributes()
{
    std::vector<pjmedia_sdp_attr*> attrs;
    attrs.reserve(SDES_OFFER_CRYPTO_SUITES.size());

    for (size_t i = 0; i < SDES_OFFER_CRYPTO_SUITES.size(); ++i)
        attrs.emplace_back(generateSdesAttribute(std::to_string(i + 1), SDES_OFFER_CRYPTO_SUITES[i]));

    return attrs;
}

pjmedia_sdp_attr*
Sdp::generateDtlsFingerprintAttribute()
{
    if (localDtlsFingerprint_.empty())
        throw SdpException("Missing DTLS fingerprint");

    const auto value = localDtlsFingerprintHash_ + " " + localDtlsFingerprint_;
    pj_str_t pjValue {sip_utils::CONST_PJ_STR(value)};
    return pjmedia_sdp_attr_create(memPool_.get(), "fingerprint", &pjValue);
}

pjmedia_sdp_attr*
Sdp::generateDtlsSetupAttribute(DtlsSetup setup)
{
    if (const auto* setupName = dtlsSetupName(setup)) {
        const std::string_view setupValue {setupName};
        pj_str_t pjValue {sip_utils::CONST_PJ_STR(setupValue)};
        return pjmedia_sdp_attr_create(memPool_.get(), "setup", &pjValue);
    }

    throw SdpException("Invalid DTLS setup attribute");
}

void
Sdp::addDtlsAttributes(pjmedia_sdp_media* media, DtlsSetup setup)
{
    if (pjmedia_sdp_media_add_attr(media, generateDtlsFingerprintAttribute()) != PJ_SUCCESS)
        throw SdpException("Unable to add DTLS fingerprint attribute to media");

    if (pjmedia_sdp_media_add_attr(media, generateDtlsSetupAttribute(setup)) != PJ_SUCCESS)
        throw SdpException("Unable to add DTLS setup attribute to media");
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
Sdp::getMediaTransport(const pjmedia_sdp_media* media)
{
    if (pj_stricmp2(&media->desc.transport, "RTP/SAVP") == 0)
        return MediaTransport::RTP_SAVP;
    else if (pj_stricmp2(&media->desc.transport, "UDP/TLS/RTP/SAVP") == 0)
        return MediaTransport::UDP_TLS_RTP_SAVP;
    else if (pj_stricmp2(&media->desc.transport, "UDP/TLS/RTP/SAVPF") == 0)
        return MediaTransport::UDP_TLS_RTP_SAVPF;
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
Sdp::addMediaDescription(const MediaAttribute& mediaAttr,
                         const pjmedia_sdp_session* previousLocalSession,
                         bool legacySdesOffer)
{
    auto type = mediaAttr.type_;
    auto secure = mediaAttr.secure_;
    const auto useBundle = shouldUseBundle();
    const auto mediaIndex = localSession_ ? localSession_->media_count : 0;
    const auto* remoteMedia = sdpDirection_ == SdpDirection::ANSWER and remoteSession_
                                      and mediaIndex < remoteSession_->media_count
                                  ? remoteSession_->media[mediaIndex]
                                  : nullptr;
    const auto* previousLocalMedia = sdpDirection_ == SdpDirection::OFFER and previousLocalSession
                                             and mediaIndex < previousLocalSession->media_count
                                         ? previousLocalSession->media[mediaIndex]
                                         : nullptr;
    const auto* codecTemplate = remoteMedia ? remoteMedia : previousLocalMedia;

    JAMI_LOG("Add media description [{}]", mediaAttr.toString(true));

    pjmedia_sdp_media* med = PJ_POOL_ZALLOC_T(memPool_.get(), pjmedia_sdp_media);

    switch (type) {
    case MediaType::MEDIA_AUDIO:
        med->desc.media = sip_utils::CONST_PJ_STR("audio");
        med->desc.port = mediaAttr.enabled_ ? localAudioRtpPort_ : 0;
        med->desc.fmt_count = audio_codec_list_.size();
        break;
    case MediaType::MEDIA_VIDEO:
        med->desc.media = sip_utils::CONST_PJ_STR("video");
        med->desc.port = mediaAttr.enabled_ ? localVideoRtpPort_ : 0;
        med->desc.fmt_count = video_codec_list_.size();
        break;
    default:
        throw SdpException("Unsupported media type! Only audio and video are supported");
        break;
    }

    med->desc.port_count = 1;

    // Set the transport protocol of the media
    if (previousLocalMedia) {
        pj_strdup(memPool_.get(), &med->desc.transport, &previousLocalMedia->desc.transport);
    } else if (secure) {
        med->desc.transport = secureMediaKeyExchange_ == KeyExchangeProtocol::DTLS
                                  ? sip_utils::CONST_PJ_STR("UDP/TLS/RTP/SAVPF")
                                  : sip_utils::CONST_PJ_STR("RTP/SAVP");
    } else {
        med->desc.transport = sip_utils::CONST_PJ_STR("RTP/AVP");
    }

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
            if (useBundle && payload >= 96 && isBundlePayloadTypeUsed(payload))
                payload = nextBundleDynamicPayloadType(payload);
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
            payload = useBundle ? nextBundleDynamicPayloadType(dynamic_payload) : dynamic_payload;
            dynamic_payload = payload + 1;
            enc_name = video_codec_list_[i]->name;
            rtpmap.clock_rate = 90000;
        }

        if (const auto offeredPayload = findOfferedPayloadType(codecTemplate, enc_name, rtpmap.clock_rate))
            payload = *offeredPayload;

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
            const auto accountVideoCodec = std::static_pointer_cast<SystemVideoCodecInfo>(video_codec_list_[i]);
            std::string profileLevelID = accountVideoCodec->parameters.empty()
                                             ? libav_utils::DEFAULT_H264_PROFILE_LEVEL_ID
                                             : accountVideoCodec->parameters;
            // RFC 6184 5.4: without packetization-mode the single NAL unit
            // mode (0) is assumed, but our RTP payloader emits FU-A fragments
            // for NALs larger than the MTU (mode 1 behavior). Advertise mode 1
            // so strict receivers such as browsers accept fragmented keyframes.
            if (profileLevelID.find("packetization-mode=") == std::string::npos)
                profileLevelID += ";packetization-mode=1";
            auto value = fmt::format("fmtp:{} {}", payload, profileLevelID);
            med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_.get(), value.c_str(), NULL);
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

    const auto remoteAllowsRtcpMux = sdpDirection_ != SdpDirection::ANSWER
                                     || (remoteSession_ and mediaIndex < remoteSession_->media_count
                                         and hasRtcpMuxAttribute(remoteSession_->media[mediaIndex]));

    if (mediaAttr.enabled_ and rtcpMuxEnabled_ and remoteAllowsRtcpMux)
        addRTCPMuxAttribute(med);

    char const* direction = mediaDirection(mediaAttr);

    med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_.get(), direction, NULL);

    if (useBundle && mediaAttr.enabled_) {
        auto mid = !mediaAttr.label_.empty() ? mediaAttr.label_ : std::to_string(mediaIndex);
        if (previousLocalMedia) {
            if (auto previousMid = getMidValue(previousLocalMedia); not previousMid.empty())
                mid = std::move(previousMid);
        } else if (sdpDirection_ == SdpDirection::ANSWER && remoteSession_
                   && mediaIndex < remoteSession_->media_count) {
            if (auto remoteMid = getMidValue(remoteSession_->media[mediaIndex]); not remoteMid.empty())
                mid = std::move(remoteMid);
        }

        addMidAttribute(med, mid);

        unsigned midExtmapId = DEFAULT_MID_RTP_EXTENSION_ID;
        const auto* extmapSession = sdpDirection_ == SdpDirection::ANSWER ? remoteSession_ : previousLocalSession;
        if (extmapSession) {
            midExtmapId = hasBundleGroup(extmapSession)
                              ? getBundleExtmapId(extmapSession, MID_RTP_EXTENSION_URI)
                              : getExtmapId(extmapSession, mediaIndex, MID_RTP_EXTENSION_URI);
        }

        if (midExtmapId != 0)
            addExtmapAttribute(med, midExtmapId, MID_RTP_EXTENSION_URI);
    }

    if (mediaAttr.enabled_ and direction != DIRECTION_STR[MediaDirection::INACTIVE]) {
        auto transportCcExtmapId = DEFAULT_TRANSPORT_CC_RTP_EXTENSION_ID;
        auto transportCcFeedback = sdpDirection_ != SdpDirection::ANSWER and not previousLocalSession;
        auto googRembFeedback = sdpDirection_ != SdpDirection::ANSWER and not previousLocalSession;

        const auto* feedbackSession = sdpDirection_ == SdpDirection::ANSWER ? remoteSession_ : previousLocalSession;
        if (feedbackSession) {
            transportCcExtmapId = hasBundleGroup(feedbackSession)
                                      ? getBundleExtmapId(feedbackSession,
                                                          TRANSPORT_CC_RTP_EXTENSION_URIS,
                                                          "transport-cc")
                                      : getExtmapId(feedbackSession,
                                                    mediaIndex,
                                                    TRANSPORT_CC_RTP_EXTENSION_URIS);
            transportCcFeedback = transportCcExtmapId != 0
                                  && hasAnyRtcpFeedback(feedbackSession, mediaIndex, "transport-cc");
            googRembFeedback = hasAnyRtcpFeedback(feedbackSession, mediaIndex, "goog-remb");
        }

        if (transportCcFeedback && transportCcExtmapId != 0) {
            addExtmapAttribute(med, transportCcExtmapId, TRANSPORT_CC_RTP_EXTENSION_URI);
            addRtcpFeedbackAttribute(med, "*", "transport-cc");
        }
        if (googRembFeedback)
            addRtcpFeedbackAttribute(med, "*", "goog-remb");

        if (type == MediaType::MEDIA_VIDEO) {
            auto nackFeedback = sdpDirection_ != SdpDirection::ANSWER and not previousLocalSession;
            auto pliFeedback = sdpDirection_ != SdpDirection::ANSWER and not previousLocalSession;
            auto firFeedback = sdpDirection_ != SdpDirection::ANSWER and not previousLocalSession;
            if (feedbackSession) {
                nackFeedback = hasAnyExactRtcpFeedback(feedbackSession, mediaIndex, "nack");
                pliFeedback = hasAnyRtcpFeedback(feedbackSession, mediaIndex, "nack pli");
                firFeedback = hasAnyRtcpFeedback(feedbackSession, mediaIndex, "ccm fir");
            }
            if (nackFeedback)
                addRtcpFeedbackAttribute(med, "*", "nack");
            if (pliFeedback)
                addRtcpFeedbackAttribute(med, "*", "nack pli");
            if (firFeedback)
                addRtcpFeedbackAttribute(med, "*", "ccm fir");
        }
    }

    if (secure and sdpDirection_ == SdpDirection::OFFER) {
        if ((previousLocalMedia and isDtlsTransport(getMediaTransport(previousLocalMedia)))
            or secureMediaKeyExchange_ == KeyExchangeProtocol::DTLS) {
            addDtlsAttributes(med, DtlsSetup::ACTPASS);
        } else {
            // Offer SDES for the SDES and default key exchanges. Jami accounts
            // enable SRTP without reporting an explicit key exchange, so the
            // default must still advertise SDES crypto for backward
            // compatibility with existing Jami and SIP peers.
            if (legacySdesOffer) {
                if (pjmedia_sdp_media_add_attr(
                        med, generateSdesAttribute("1", "AES_CM_128_HMAC_SHA1_80"))
                    != PJ_SUCCESS) {
                    throw SdpException("Unable to add legacy sdes attribute to media");
                }
            } else {
                for (auto* attr : generateSdesOfferAttributes()) {
                    if (pjmedia_sdp_media_add_attr(med, attr) != PJ_SUCCESS)
                        throw SdpException("Unable to add sdes attribute to media");
                }
            }
            // Hybrid SDES+DTLS offer (RFC 5764 4.1): keep the SDES-compatible
            // transport but also advertise our DTLS identity so the answerer
            // can pick DTLS-SRTP. SDES-only peers ignore these attributes.
            if (not localDtlsFingerprint_.empty())
                addDtlsAttributes(med, DtlsSetup::ACTPASS);
        }
    }

    return med;
}

bool
Sdp::shouldUseBundle() const
{
    if (!bundleEnabled_)
        return false;

    return sdpDirection_ != SdpDirection::ANSWER || hasBundleGroup(remoteSession_);
}

bool
Sdp::isBundlePayloadTypeUsed(unsigned payloadType) const
{
    if (payloadType == telephoneEventPayload_)
        return true;

    if (!shouldUseBundle() || !localSession_)
        return false;

    for (unsigned mediaIdx = 0; mediaIdx < localSession_->media_count; ++mediaIdx) {
        auto* media = localSession_->media[mediaIdx];
        for (unsigned fmtIdx = 0; fmtIdx < media->desc.fmt_count; ++fmtIdx) {
            if (pj_strtoul(&media->desc.fmt[fmtIdx]) == payloadType)
                return true;
        }
    }

    return false;
}

unsigned
Sdp::nextBundleDynamicPayloadType(unsigned startPayloadType) const
{
    auto payloadType = std::max(96u, startPayloadType);
    while (payloadType < 128 && isBundlePayloadTypeUsed(payloadType))
        ++payloadType;

    if (payloadType >= 128)
        throw SdpException("No dynamic RTP payload type available for bundled media");

    return payloadType;
}

void
Sdp::addSdesAttribute(pjmedia_sdp_media* media, const std::vector<std::string>& crypto)
{
    const auto selected = SdesNegotiator::negotiate(crypto);
    if (not selected)
        throw SdpException("Unable to negotiate sdes attribute for media");

    auto* attr = generateSdesAttribute(selected.getTag(), selected.getCryptoSuite());
    if (pjmedia_sdp_media_add_attr(media, attr) != PJ_SUCCESS)
        throw SdpException("Unable to add sdes attribute to media");
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
Sdp::addRTCPMuxAttribute(pjmedia_sdp_media* med)
{
    if (pjmedia_sdp_media_add_attr(med, pjmedia_sdp_attr_create(memPool_.get(), "rtcp-mux", nullptr)) != PJ_SUCCESS) {
        throw SdpException("Unable to add rtcp-mux attribute to media");
    }
}

void
Sdp::addMidAttribute(pjmedia_sdp_media* med, std::string_view mid)
{
    const pj_str_t value = sip_utils::CONST_PJ_STR(mid);
    if (pjmedia_sdp_media_add_attr(med, pjmedia_sdp_attr_create(memPool_.get(), "mid", &value)) != PJ_SUCCESS)
        throw SdpException("Unable to add mid attribute to media");
}

void
Sdp::addExtmapAttribute(pjmedia_sdp_media* med, unsigned id, std::string_view uri)
{
    const auto value = fmt::format("{} {}", id, uri);
    const pj_str_t pjValue = sip_utils::CONST_PJ_STR(value);
    if (pjmedia_sdp_media_add_attr(med, pjmedia_sdp_attr_create(memPool_.get(), "extmap", &pjValue)) != PJ_SUCCESS) {
        throw SdpException("Unable to add extmap attribute to media");
    }
}

void
Sdp::addRtcpFeedbackAttribute(pjmedia_sdp_media* med, std::string_view payloadType, std::string_view feedbackType)
{
    const auto value = fmt::format("{} {}", payloadType, feedbackType);
    const pj_str_t pjValue = sip_utils::CONST_PJ_STR(value);
    if (pjmedia_sdp_media_add_attr(med, pjmedia_sdp_attr_create(memPool_.get(), "rtcp-fb", &pjValue)) != PJ_SUCCESS) {
        throw SdpException("Unable to add rtcp-fb attribute to media");
    }
}

void
Sdp::addBundleGroupAttribute()
{
    if (!shouldUseBundle() || !localSession_)
        return;

    // Answers must echo the offered BUNDLE group (RFC 9143), even with a
    // single media. Offers only use BUNDLE when grouping several medias.
    const bool answerToBundle = sdpDirection_ == SdpDirection::ANSWER && hasBundleGroup(remoteSession_);
    if (localSession_->media_count < 2 && !answerToBundle)
        return;

    std::string mids {"BUNDLE"};
    for (unsigned i = 0; i < localSession_->media_count; ++i) {
        auto* media = localSession_->media[i];
        auto* midAttr = pjmedia_sdp_attr_find2(media->attr_count, media->attr, "mid", nullptr);
        if (!midAttr)
            continue;

        mids.append(" ");
        mids.append(midAttr->value.ptr, midAttr->value.slen);
    }

    const pj_str_t value = sip_utils::CONST_PJ_STR(mids);
    if (pjmedia_sdp_attr_add(&localSession_->attr_count,
                             localSession_->attr,
                             pjmedia_sdp_attr_create(memPool_.get(), "group", &value))
        != PJ_SUCCESS) {
        throw SdpException("Unable to add BUNDLE group attribute to session");
    }
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
    static constexpr size_t BUF_SZ = 16*1024 - 1;
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
Sdp::createOffer(const std::vector<MediaAttribute>& mediaList,
                 const pjmedia_sdp_session* previousLocalSession,
                 bool legacySdesOffer)
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
            localSession_->media[localSession_->media_count++]
                = addMediaDescription(media, previousLocalSession, legacySdesOffer);
        }
    }

    addBundleGroupAttribute();

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

static pjmedia_sdp_session*
parseExternalSdp(pj_pool_t* pool, const std::string& sdp)
{
    pjmedia_sdp_session* session = nullptr;
    // The parsed session keeps pointers into the buffer, so it must
    // live as long as the pool.
    auto* buffer = static_cast<char*>(pj_pool_alloc(pool, sdp.size()));
    std::memcpy(buffer, sdp.data(), sdp.size());
    if (pjmedia_sdp_parse(pool, buffer, sdp.size(), &session) != PJ_SUCCESS) {
        JAMI_ERROR("Failed to parse external SDP session");
        return nullptr;
    }
    if (pjmedia_sdp_validate(session) != PJ_SUCCESS) {
        JAMI_ERROR("Invalid external SDP session");
        return nullptr;
    }
    return session;
}

bool
Sdp::createOfferFromExternalSdp(const std::string& sdp)
{
    auto* session = parseExternalSdp(memPool_.get(), sdp);
    if (not session)
        return false;

    sdpDirection_ = SdpDirection::OFFER;
    localSession_ = session;

    if (pjmedia_sdp_neg_create_w_local_offer(memPool_.get(), localSession_, &negotiator_) != PJ_SUCCESS) {
        JAMI_ERROR("Failed to create an initial SDP negotiator");
        return false;
    }

    printSession(localSession_, "Local session (external):", sdpDirection_);
    return true;
}

bool
Sdp::setLocalAnswerFromExternalSdp(const std::string& sdp)
{
    if (not remoteSession_) {
        JAMI_ERROR("No remote offer to answer");
        return false;
    }

    auto* session = parseExternalSdp(memPool_.get(), sdp);
    if (not session)
        return false;

    sdpDirection_ = SdpDirection::ANSWER;
    localSession_ = session;

    if (pjmedia_sdp_neg_create_w_remote_offer(memPool_.get(), localSession_, remoteSession_, &negotiator_)
        != PJ_SUCCESS) {
        JAMI_ERROR("Failed to initialize media negotiation");
        return false;
    }

    printSession(localSession_, "Local session (external answer):", sdpDirection_);
    return true;
}

std::string
Sdp::toString(const pjmedia_sdp_session* session)
{
    if (not session)
        return {};
    static constexpr size_t BUF_SZ = 16 * 1024;
    std::vector<char> buffer(BUF_SZ);
    auto size = pjmedia_sdp_print(session, buffer.data(), buffer.size());
    if (size < 0) {
        JAMI_ERROR("Failed to serialize SDP session");
        return {};
    }
    return std::string(buffer.data(), size);
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

    addBundleGroupAttribute();

    for (unsigned i = 0; i < localSession_->media_count and i < remoteSession_->media_count; ++i) {
        auto* localMedia = localSession_->media[i];
        auto* remoteMedia = remoteSession_->media[i];

        const auto localTransport = getMediaTransport(localMedia);
        const auto remoteTransport = getMediaTransport(remoteMedia);

        if (localTransport == MediaTransport::RTP_SAVP) {
            // Prefer DTLS-SRTP when a hybrid offer (RFC 5764 4.1) carries a
            // fingerprint and we have a DTLS identity of our own.
            const auto remoteFingerprint = getDtlsFingerprint(remoteSession_, i);
            if (not localDtlsFingerprint_.empty() and not remoteFingerprint.second.empty()) {
                // The answer must echo the offered transport (RFC 3264 6.),
                // e.g. UDP/TLS/RTP/SAVPF when offered by a WebRTC endpoint.
                pj_strdup(memPool_.get(), &localMedia->desc.transport, &remoteMedia->desc.transport);
                addDtlsAttributes(localMedia,
                                  negotiateLocalDtlsSetup(DtlsSetup::ACTPASS, getDtlsSetup(remoteSession_, i)));
                continue;
            }

            const auto crypto = getCrypto(remoteMedia);
            if (crypto.empty())
                continue;

            addSdesAttribute(localMedia, crypto);
            continue;
        }

        if (not isDtlsTransport(localTransport) || not isDtlsTransport(remoteTransport))
            continue;

        const auto remoteFingerprint = getDtlsFingerprint(remoteSession_, i);
        if (remoteFingerprint.second.empty())
            continue;

        addDtlsAttributes(localMedia, negotiateLocalDtlsSetup(DtlsSetup::ACTPASS, getDtlsSetup(remoteSession_, i)));
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
        const auto useLocalSecurityFallback = not remote and session == activeLocalSession_ and localSession_
                                              and i < localSession_->media_count;
        const auto* securitySession = useLocalSecurityFallback ? localSession_ : session;
        auto* securityMedia = useLocalSecurityFallback ? localSession_->media[i] : media;
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
        descr.rtcp_mux = hasRtcpMuxAttribute(media);

        if (descr.rtcp_mux) {
            descr.rtcp_addr = descr.addr;
        } else {
            // Get the "rtcp" address from the SDP if present. Otherwise,
            // infer it from the RTP endpoint address using RTP+1.
            auto* attr = pjmedia_sdp_attr_find2(media->attr_count, media->attr, "rtcp", NULL);
            if (attr) {
                pjmedia_sdp_rtcp_attr rtcp;
                auto status = pjmedia_sdp_attr_get_rtcp(attr, &rtcp);
                if (status == PJ_SUCCESS) {
                    if (rtcp.addr.slen) {
                        descr.rtcp_addr = std::string_view(rtcp.addr.ptr, rtcp.addr.slen);
                    } else {
                        descr.rtcp_addr = descr.addr;
                    }
                    descr.rtcp_addr.setPort(rtcp.port);
                }
            }

            if (!descr.rtcp_addr) {
                descr.rtcp_addr = descr.addr;
                descr.rtcp_addr.setPort(descr.addr.getPort() + 1);
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
        descr.transport = getMediaTransport(media);
        descr.mid = getMidValue(media);
        descr.mid_rtp_ext_id = hasBundleGroup(session) ? getBundleExtmapId(session, MID_RTP_EXTENSION_URI)
                                                       : getExtmapId(session, i, MID_RTP_EXTENSION_URI);
        descr.transport_cc_rtp_ext_id = hasBundleGroup(session)
                                            ? getBundleExtmapId(session, TRANSPORT_CC_RTP_EXTENSION_URIS, "transport-cc")
                                            : getExtmapId(session, i, TRANSPORT_CC_RTP_EXTENSION_URIS);

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
                if (fmtpAttr && fmtpAttr->value.ptr && fmtpAttr->value.slen) {
                    const auto& v = fmtpAttr->value;
                    descr.parameters = std::string(v.ptr, v.ptr + v.slen);
                }
            }
            // for now, just keep the first codec only
            descr.enabled = true;
            break;
        }

        if (descr.enabled) {
            descr.rtcp_fb_transport_cc = hasRtcpFeedback(session, i, descr.payload_type, "transport-cc");
            descr.rtcp_fb_goog_remb = hasRtcpFeedback(session, i, descr.payload_type, "goog-remb");
            descr.rtcp_fb_nack_pli = hasRtcpFeedback(session, i, descr.payload_type, "nack pli");
            descr.rtcp_fb_ccm_fir = hasRtcpFeedback(session, i, descr.payload_type, "ccm fir");
        }

        if (not remote)
            descr.receiving_sdp = getFilteredSdp(session, i, descr.payload_type);

        bool useDtls = isDtlsTransport(descr.transport);
        if (not useDtls and not getDtlsFingerprint(securitySession, i).second.empty()) {
            // Hybrid SDES+DTLS m-line (RFC 5764 4.1): DTLS is in use when the
            // counterpart session also committed to it with a fingerprint.
            const auto* counterpart = remote ? (activeLocalSession_ ? activeLocalSession_ : localSession_)
                                             : activeRemoteSession_;
            if (counterpart and i < counterpart->media_count)
                useDtls = not getDtlsFingerprint(counterpart, i).second.empty();
            else
                useDtls = getCrypto(media).empty();
        }

        if (useDtls) {
            descr.key_exchange = KeyExchangeProtocol::DTLS;
            auto [fingerprintType, fingerprint] = getDtlsFingerprint(securitySession, i);
            descr.dtls_fingerprint_type = std::move(fingerprintType);
            descr.dtls_fingerprint = std::move(fingerprint);
            descr.dtls_setup = getDtlsSetup(securitySession, i);
            if (not remote and session == activeLocalSession_ and activeRemoteSession_
                and i < activeRemoteSession_->media_count) {
                descr.dtls_setup = negotiateLocalDtlsSetup(descr.dtls_setup, getDtlsSetup(activeRemoteSession_, i));
            }
            continue;
        }

        // get crypto info
        std::vector<std::string> crypto;
        for (unsigned j = 0; j < securityMedia->attr_count; j++) {
            auto* const attribute = securityMedia->attr[j];
            if (pj_stricmp2(&attribute->name, "crypto") == 0)
                crypto.emplace_back(attribute->value.ptr, attribute->value.slen);
        }
        if (not remote and session == activeLocalSession_ and activeRemoteSession_
            and i < activeRemoteSession_->media_count) {
            const auto remoteCrypto = getCrypto(activeRemoteSession_->media[i]);
            const auto selectedRemoteCrypto = SdesNegotiator::negotiate(remoteCrypto);
            if (selectedRemoteCrypto) {
                // Match on the crypto suite only: a peer selecting one of our
                // offered suites may answer with a different tag than the one we
                // used (e.g. an older Jami hardcodes tag 1), so the local tag
                // must not take part in the comparison.
                const auto& remoteSuite = selectedRemoteCrypto.getCryptoSuite();
                const auto matchingLocalCrypto
                    = std::find_if(crypto.begin(), crypto.end(), [&remoteSuite](const auto& item) {
                          const auto local = SdesNegotiator::negotiate(std::vector<std::string> {item});
                          return local and local.getCryptoSuite() == remoteSuite;
                      });
                if (matchingLocalCrypto != crypto.end()) {
                    descr.key_exchange = KeyExchangeProtocol::SDES;
                    descr.crypto = SdesNegotiator::negotiate(std::vector<std::string> {*matchingLocalCrypto});
                    continue;
                }
            }
        }

        if (not crypto.empty())
            descr.key_exchange = KeyExchangeProtocol::SDES;
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

        const auto dtlsFingerprint = getDtlsFingerprint(sdpSession, idx);
        mediaAttr.secure_ = (transp == MediaTransport::RTP_SAVP and not getCrypto(media).empty())
                            || (isDtlsTransport(transp) and not dtlsFingerprint.second.empty());

        if (mediaAttr.type_ == MediaType::MEDIA_AUDIO) {
            mediaAttr.label_ = "audio_" + std::to_string(audioIdx++);
        } else if (mediaAttr.type_ == MediaType::MEDIA_VIDEO) {
            mediaAttr.label_ = "video_" + std::to_string(videoIdx++);
        }
    }

    return mediaList;
}

} // namespace jami
