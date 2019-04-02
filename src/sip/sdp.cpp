/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
using std::map;
using std::vector;
using std::stringstream;

static constexpr int POOL_INITIAL_SIZE = 16384;
static constexpr int POOL_INCREMENT_SIZE = POOL_INITIAL_SIZE;

Sdp::Sdp(const std::string& id)
    : memPool_(nullptr, [](pj_pool_t* pool) { sip_utils::register_thread(); pj_pool_release(pool); })
    , publishedIpAddr_()
    , publishedIpAddrType_()
    , sdesNego_ {CryptoSuites}
    , telephoneEventPayload_(101) // same as asterisk
{
    sip_utils::register_thread();
    memPool_.reset(pj_pool_create(&getSIPVoIPLink()->getCachingPool()->factory,
                                  id.c_str(), POOL_INITIAL_SIZE,
                                  POOL_INCREMENT_SIZE, NULL));
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
Sdp::findCodecBySpec(const std::string &codec, const unsigned clockrate) const
{
    //TODO : only manage a list?
    for (const auto& accountCodec : audio_codec_list_) {
        auto audioCodecInfo = std::static_pointer_cast<AccountAudioCodecInfo>(accountCodec);
        auto& sysCodecInfo = *static_cast<const SystemAudioCodecInfo*>(&audioCodecInfo->systemCodecInfo);
        if (sysCodecInfo.name.compare(codec) == 0 and
            (audioCodecInfo->isPCMG722() ?
                (clockrate == 8000) :
                (sysCodecInfo.audioformat.sample_rate == clockrate)))
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
    //TODO : only manage a list?
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
    std::uniform_int_distribution<int> rand_byte{ 0, std::numeric_limits<uint8_t>::max() };
    random_device rdev;
    std::generate(dest.begin(), dest.end(), std::bind(rand_byte, std::ref(rdev)));
}


void
Sdp::setActiveLocalSdpSession(const pjmedia_sdp_session* sdp)
{
    activeLocalSession_ = sdp;
}

void
Sdp::setActiveRemoteSdpSession(const pjmedia_sdp_session *sdp)
{
    if (!sdp) {
        JAMI_ERR("Remote sdp is NULL");
        return;
    }

    activeRemoteSession_ = sdp;
}

pjmedia_sdp_attr *
Sdp::generateSdesAttribute()
{
    static constexpr const unsigned cryptoSuite = 0;
    std::vector<uint8_t> keyAndSalt;
    keyAndSalt.resize(jami::CryptoSuites[cryptoSuite].masterKeyLength / 8
                    + jami::CryptoSuites[cryptoSuite].masterSaltLength/ 8);
    // generate keys
    randomFill(keyAndSalt);

    std::string tag = "1";
    std::string crypto_attr = tag + " "
                            + jami::CryptoSuites[cryptoSuite].name
                            + " inline:" + base64::encode(keyAndSalt);
    pj_str_t val { (char*) crypto_attr.c_str(),
                    static_cast<pj_ssize_t>(crypto_attr.size()) };
    return pjmedia_sdp_attr_create(memPool_.get(), "crypto", &val);
}

pjmedia_sdp_media *
Sdp::setMediaDescriptorLines(bool audio, bool holding, sip_utils::KeyExchangeProtocol kx)
{
    pjmedia_sdp_media *med = PJ_POOL_ZALLOC_T(memPool_.get(), pjmedia_sdp_media);

    med->desc.media = audio ? pj_str((char*) "audio") : pj_str((char*) "video");
    med->desc.port_count = 1;
    med->desc.port = audio ? localAudioDataPort_ : localVideoDataPort_;

    // in case of sdes, media are tagged as "RTP/SAVP", RTP/AVP elsewhere
    med->desc.transport = pj_str(kx == sip_utils::KeyExchangeProtocol::NONE ?
        (char*) "RTP/AVP" :
        (char*) "RTP/SAVP");

    unsigned dynamic_payload = 96;

    med->desc.fmt_count = audio ? audio_codec_list_.size() : video_codec_list_.size();
    for (unsigned i = 0; i < med->desc.fmt_count; ++i) {
        pjmedia_sdp_rtpmap rtpmap;
        rtpmap.param.slen = 0;

        std::string channels; // must have the lifetime of rtpmap
        std::string enc_name;
        unsigned payload;

        if (audio) {
            auto accountAudioCodec = std::static_pointer_cast<AccountAudioCodecInfo>(audio_codec_list_[i]);
            payload = accountAudioCodec->payloadType;
            enc_name = accountAudioCodec->systemCodecInfo.name;

            if (accountAudioCodec->audioformat.nb_channels > 1) {
                channels = jami::to_string(accountAudioCodec->audioformat.nb_channels);
                rtpmap.param.ptr = (char *) channels.c_str();
                rtpmap.param.slen = strlen(channels.c_str()); // don't include NULL terminator
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

        std::ostringstream s;
        s << payload;
        pj_strdup2(memPool_.get(), &med->desc.fmt[i], s.str().c_str());

        // Add a rtpmap field for each codec
        // We could add one only for dynamic payloads because the codecs with static RTP payloads
        // are entirely defined in the RFC 3351
        rtpmap.pt = med->desc.fmt[i];
        rtpmap.enc_name = pj_str((char*) enc_name.c_str());

        pjmedia_sdp_attr *attr;
        pjmedia_sdp_rtpmap_to_attr(memPool_.get(), &rtpmap, &attr);
        med->attr[med->attr_count++] = attr;

#ifdef ENABLE_VIDEO
        if (enc_name == "H264") {
            // FIXME: this should not be hardcoded, it will determine what profile and level
            // our peer will send us
            const auto accountVideoCodec = std::static_pointer_cast<AccountVideoCodecInfo>(video_codec_list_[i]);
            const auto profileLevelID = accountVideoCodec->parameters.empty() ?
                libav_utils::DEFAULT_H264_PROFILE_LEVEL_ID :
                accountVideoCodec->parameters;
            std::ostringstream os;
            os << "fmtp:" << payload << " " << profileLevelID;
            med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_.get(), os.str().c_str(), NULL);
        }
#endif
    }

    if (audio) {
        setTelephoneEventRtpmap(med);
        addRTCPAttribute(med); // video has its own RTCP
    }

    med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_.get(), holding ? (audio ? "sendonly" : "inactive") : "sendrecv", NULL);

    if (kx == sip_utils::KeyExchangeProtocol::SDES) {
        if (pjmedia_sdp_media_add_attr(med, generateSdesAttribute()) != PJ_SUCCESS)
            throw SdpException("Could not add sdes attribute to media");
    }

    return med;
}


void Sdp::addRTCPAttribute(pjmedia_sdp_media *med)
{
    IpAddr outputAddr = publishedIpAddr_;
    outputAddr.setPort(localAudioControlPort_);
    pjmedia_sdp_attr *attr = pjmedia_sdp_attr_create_rtcp(memPool_.get(), outputAddr.pjPtr());
    if (attr)
        pjmedia_sdp_attr_add(&med->attr_count, med->attr, attr);
}

void
Sdp::setPublishedIP(const std::string &addr, pj_uint16_t addr_type)
{
    publishedIpAddr_ = addr;
    publishedIpAddrType_ = addr_type;
    if (localSession_) {
        if (addr_type == pj_AF_INET6())
            localSession_->origin.addr_type = pj_str((char*) "IP6");
        else
            localSession_->origin.addr_type = pj_str((char*) "IP4");
        localSession_->origin.addr = pj_str((char*) publishedIpAddr_.c_str());
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

void Sdp::setTelephoneEventRtpmap(pjmedia_sdp_media *med)
{
    std::ostringstream s;
    s << telephoneEventPayload_;
    ++med->desc.fmt_count;
    pj_strdup2(memPool_.get(), &med->desc.fmt[med->desc.fmt_count - 1], s.str().c_str());

    pjmedia_sdp_attr *attr_rtpmap = static_cast<pjmedia_sdp_attr *>(pj_pool_zalloc(memPool_.get(), sizeof(pjmedia_sdp_attr)));
    attr_rtpmap->name = pj_str((char *) "rtpmap");
    attr_rtpmap->value = pj_str((char *) "101 telephone-event/8000");

    med->attr[med->attr_count++] = attr_rtpmap;

    pjmedia_sdp_attr *attr_fmtp = static_cast<pjmedia_sdp_attr *>(pj_pool_zalloc(memPool_.get(), sizeof(pjmedia_sdp_attr)));
    attr_fmtp->name = pj_str((char *) "fmtp");
    attr_fmtp->value = pj_str((char *) "101 0-15");

    med->attr[med->attr_count++] = attr_fmtp;
}

void Sdp::setLocalMediaVideoCapabilities(const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedCodecs)
{
#ifdef ENABLE_VIDEO
    video_codec_list_ = selectedCodecs;
#else
    (void) selectedCodecs;
#endif
}

void Sdp::setLocalMediaAudioCapabilities(const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedCodecs)
{
    audio_codec_list_ = selectedCodecs;
}

void
Sdp::printSession(const pjmedia_sdp_session *session, const char* header)
{
    static constexpr size_t BUF_SZ = 4095;
    sip_utils::register_thread();
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> tmpPool_(
        pj_pool_create(&getSIPVoIPLink()->getCachingPool()->factory, "printSdp", BUF_SZ, BUF_SZ, nullptr),
        pj_pool_release
    );

    auto cloned_session = pjmedia_sdp_session_clone(tmpPool_.get(), session);
    if (!cloned_session) {
        JAMI_ERR("Could not clone SDP for printing");
        return;
    }

    // Filter-out sensible data like SRTP master key.
    for (unsigned i = 0; i < cloned_session->media_count; ++i) {
        pjmedia_sdp_media_remove_all_attr(cloned_session->media[i], "crypto");
    }

    std::array<char, BUF_SZ+1> buffer;
    auto size = pjmedia_sdp_print(cloned_session, buffer.data(), BUF_SZ);
    if (size < 0) {
        JAMI_ERR("%sSDP too big for dump", header);
        return;
    }
    buffer[size] = '\0';
    JAMI_DBG("%s%s", header, &buffer[0]);
}

int Sdp::createLocalSession(const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedAudioCodecs,
                            const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedVideoCodecs,
                            sip_utils::KeyExchangeProtocol security,
                            bool holding)
{
    setLocalMediaAudioCapabilities(selectedAudioCodecs);
    setLocalMediaVideoCapabilities(selectedVideoCodecs);

    localSession_ = PJ_POOL_ZALLOC_T(memPool_.get(), pjmedia_sdp_session);
    localSession_->conn = PJ_POOL_ZALLOC_T(memPool_.get(), pjmedia_sdp_conn);

    /* Initialize the fields of the struct */
    localSession_->origin.version = 0;
    pj_time_val tv;
    pj_gettimeofday(&tv);

    localSession_->origin.user = pj_str(pj_gethostname()->ptr);
    // Use Network Time Protocol format timestamp to ensure uniqueness.
    localSession_->origin.id = tv.sec + 2208988800UL;
    localSession_->origin.net_type = pj_str((char*) "IN");
    if (publishedIpAddrType_ == pj_AF_INET6())
        localSession_->origin.addr_type = pj_str((char*) "IP6");
    else
        localSession_->origin.addr_type = pj_str((char*) "IP4");
    localSession_->origin.addr = pj_str((char*) publishedIpAddr_.c_str());
    localSession_->name = pj_str((char*) PACKAGE_NAME);
    localSession_->conn->net_type = localSession_->origin.net_type;
    localSession_->conn->addr_type = localSession_->origin.addr_type;
    localSession_->conn->addr = localSession_->origin.addr;

    // RFC 3264: An offer/answer model session description protocol
    // As the session is created and destroyed through an external signaling mean (SIP), the line
    // should have a value of "0 0".
    localSession_->time.start = 0;
    localSession_->time.stop = 0;

    // For DTMF RTP events
    constexpr bool audio = true;
    localSession_->media_count = 1;
    localSession_->media[0] = setMediaDescriptorLines(audio, holding, security);
    if (not selectedVideoCodecs.empty()) {
        localSession_->media[1] = setMediaDescriptorLines(!audio, holding, security);
        ++localSession_->media_count;
    }

    printSession(localSession_, "SDP: Local SDP Session:\n");

    return pjmedia_sdp_validate(localSession_);
}

bool
Sdp::createOffer(const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedAudioCodecs,
                 const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedVideoCodecs,
                 sip_utils::KeyExchangeProtocol security,
                 bool holding)
{
    if (createLocalSession(selectedAudioCodecs, selectedVideoCodecs, security, holding) != PJ_SUCCESS) {
        JAMI_ERR("Failed to create initial offer");
        return false;
    }

    if (pjmedia_sdp_neg_create_w_local_offer(memPool_.get(), localSession_, &negotiator_) != PJ_SUCCESS) {
        JAMI_ERR("Failed to create an initial SDP negotiator");
        return false;
    }

    return true;
}

void Sdp::receiveOffer(const pjmedia_sdp_session* remote,
                       const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedAudioCodecs,
                       const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedVideoCodecs,
                       sip_utils::KeyExchangeProtocol kx,
                       bool holding)
{
    if (!remote) {
        JAMI_ERR("Remote session is NULL");
        return;
    }

    printSession(remote, "Remote SDP Session:\n");

    if (not localSession_ and createLocalSession(selectedAudioCodecs,
                                                 selectedVideoCodecs, kx, holding) != PJ_SUCCESS) {
        JAMI_ERR("Failed to create initial offer");
        return;
    }

    remoteSession_ = pjmedia_sdp_session_clone(memPool_.get(), remote);

    if (pjmedia_sdp_neg_create_w_remote_offer(memPool_.get(), localSession_,
            remoteSession_, &negotiator_) != PJ_SUCCESS)
        JAMI_ERR("Failed to initialize negotiator");
}

void Sdp::startNegotiation()
{
    if (negotiator_ == NULL) {
        JAMI_ERR("Can't start negotiation with invalid negotiator");
        return;
    }

    const pjmedia_sdp_session *active_local;
    const pjmedia_sdp_session *active_remote;

    if (pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO) {
        JAMI_WARN("Negotiator not in right state for negotiation");
        return;
    }

    if (pjmedia_sdp_neg_negotiate(memPool_.get(), negotiator_, 0) != PJ_SUCCESS)
        return;

    if (pjmedia_sdp_neg_get_active_local(negotiator_, &active_local) != PJ_SUCCESS)
        JAMI_ERR("Could not retrieve local active session");
    else
        setActiveLocalSdpSession(active_local);

    if (pjmedia_sdp_neg_get_active_remote(negotiator_, &active_remote) != PJ_SUCCESS)
        JAMI_ERR("Could not retrieve remote active session");
    else
        setActiveRemoteSdpSession(active_remote);
}


std::string
Sdp::getFilteredSdp(const pjmedia_sdp_session* session, unsigned media_keep, unsigned pt_keep)
{
    sip_utils::register_thread();
    static constexpr size_t BUF_SZ = 4096;
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> tmpPool_(
        pj_pool_create(&getSIPVoIPLink()->getCachingPool()->factory,
                           "tmpSdp", BUF_SZ, BUF_SZ, nullptr),
        pj_pool_release
    );
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
            std::move(cloned->media+i+1,
                      cloned->media+cloned->media_count,
                      cloned->media+i);
            cloned->media_count--;
            i--;
        }

    for (unsigned i = 0; i < cloned->media_count; i++) {
        auto media = cloned->media[i];

        // filter other codecs
        for (unsigned c=0; c<media->desc.fmt_count; c++) {
            auto& pt = media->desc.fmt[c];
            if (pj_strtoul(&pt) == pt_keep)
                continue;

            while (auto attr = pjmedia_sdp_attr_find2(media->attr_count,
                                                      media->attr,
                                                      "rtpmap", &pt))
                pjmedia_sdp_attr_remove(&media->attr_count, media->attr, attr);

            while (auto attr = pjmedia_sdp_attr_find2(media->attr_count,
                                                      media->attr,
                                                      "fmt", &pt))
                pjmedia_sdp_attr_remove(&media->attr_count, media->attr, attr);

            std::move(media->desc.fmt+c+1,
                      media->desc.fmt+media->desc.fmt_count,
                      media->desc.fmt+c);
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
Sdp::getMediaSlots(const pjmedia_sdp_session* session, bool remote) const
{
    static constexpr pj_str_t STR_RTPMAP { (char*) "rtpmap", 6 };
    static constexpr pj_str_t STR_FMTP { (char*) "fmtp", 4 };

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
        descr.addr = std::string(conn->addr.ptr, conn->addr.slen);
        descr.addr.setPort(media->desc.port);

        descr.holding = pjmedia_sdp_attr_find2(media->attr_count, media->attr, "sendonly", nullptr)
                     || pjmedia_sdp_attr_find2(media->attr_count, media->attr, "inactive", nullptr);

        // get codecs infos
        for (unsigned j = 0; j<media->desc.fmt_count; j++) {
            const auto rtpMapAttribute = pjmedia_sdp_media_find_attr(media, &STR_RTPMAP, &media->desc.fmt[j]);
            if (!rtpMapAttribute) {
                JAMI_ERR("Could not find rtpmap attribute");
                descr.enabled = false;
                continue;
            }
            pjmedia_sdp_rtpmap rtpmap;
            if (pjmedia_sdp_attr_get_rtpmap(rtpMapAttribute, &rtpmap) != PJ_SUCCESS ||
                rtpmap.enc_name.slen == 0)
            {
                JAMI_ERR("Could not find payload type %.*s in SDP",
                        (int)media->desc.fmt[j].slen, media->desc.fmt[j].ptr);
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
                const auto fmtpAttr = pjmedia_sdp_media_find_attr(media, &STR_FMTP, &media->desc.fmt[j]);
                //descr.bitrate = getOutgoingVideoField(codec, "bitrate");
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
        descr.crypto = sdesNego_.negotiate(crypto);
    }
    return ret;
}

std::vector<Sdp::MediaSlot>
Sdp::getMediaSlots() const
{
    auto loc = getMediaSlots(activeLocalSession_, false);
    auto rem = getMediaSlots(activeRemoteSession_, true);
    size_t slot_n = std::min(loc.size(), rem.size());
    std::vector<MediaSlot> s;
    s.reserve(slot_n);
    for (decltype(slot_n) i=0; i<slot_n; i++)
        s.emplace_back(std::move(loc[i]), std::move(rem[i]));
    return s;
}

void
Sdp::addIceCandidates(unsigned media_index, const std::vector<std::string>& cands)
{
    if (media_index >= localSession_->media_count) {
        JAMI_ERR("addIceCandidates failed: cannot access media#%u (may be deactivated)", media_index);
        return;
    }

    auto media = localSession_->media[media_index];

    for (const auto &item : cands) {
        pj_str_t val = { (char*) item.c_str(), static_cast<pj_ssize_t>(item.size()) };
        pjmedia_sdp_attr *attr = pjmedia_sdp_attr_create(memPool_.get(), "candidate", &val);

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
    if (media_index >= session->media_count || media_index >= localSession->media_count) {
        JAMI_ERR("getIceCandidates failed: cannot access media#%u (may be deactivated)", media_index);
        return {};
    }
    auto media = session->media[media_index];
    auto localMedia = localSession->media[media_index];
    if (media->desc.port == 0 || localMedia->desc.port == 0) {
        JAMI_ERR("getIceCandidates failed: media#%u is disabled", media_index);
        return {};
    }

    std::vector<std::string> candidates;

    for (unsigned i=0; i < media->attr_count; i++) {
        pjmedia_sdp_attr *attribute = media->attr[i];
        if (pj_stricmp2(&attribute->name, "candidate") == 0)
            candidates.push_back(std::string(attribute->value.ptr, attribute->value.slen));
    }

    return candidates;
}

void
Sdp::addIceAttributes(const IceTransport::Attribute&& ice_attrs)
{
    pj_str_t value;
    pjmedia_sdp_attr *attr;

    value = { (char*)ice_attrs.ufrag.c_str(), static_cast<pj_ssize_t>(ice_attrs.ufrag.size()) };
    attr = pjmedia_sdp_attr_create(memPool_.get(), "ice-ufrag", &value);

    if (pjmedia_sdp_attr_add(&localSession_->attr_count, localSession_->attr, attr) != PJ_SUCCESS)
        throw SdpException("Could not add ICE.ufrag attribute to local SDP");

    value = { (char*)ice_attrs.pwd.c_str(), static_cast<pj_ssize_t>(ice_attrs.pwd.size()) };
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
    for (unsigned i=0; i < session->attr_count; i++) {
        pjmedia_sdp_attr *attribute = session->attr[i];
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
    for (unsigned i=0; i < session->media_count; i++) {
        auto media = session->media[i];
        pjmedia_sdp_attr_remove_all(&media->attr_count, media->attr, "candidate");
    }
}

} // namespace jami
