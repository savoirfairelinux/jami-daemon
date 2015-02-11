/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
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

#include <algorithm>
#include <cassert>

namespace ring {

using std::string;
using std::map;
using std::vector;
using std::stringstream;

static constexpr int POOL_INITIAL_SIZE = 16384;
static constexpr int POOL_INCREMENT_SIZE = POOL_INITIAL_SIZE;

Sdp::Sdp(const std::string& id)
    : memPool_(nullptr, pj_pool_release)
    , negotiator_(nullptr)
    , localSession_(nullptr)
    , remoteSession_(nullptr)
    , activeLocalSession_(nullptr)
    , activeRemoteSession_(nullptr)
    , audio_codec_list_()
    , video_codec_list_()
    , sessionAudioMediaLocal_()
    , sessionAudioMediaRemote_()
    , sessionVideoMedia_()
    , publishedIpAddr_()
    , publishedIpAddrType_()
    , localAudioDataPort_(0)
    , localAudioControlPort_(0)
    , localVideoDataPort_(0)
    , localVideoControlPort_(0)
    , zrtpHelloHash_()
    , telephoneEventPayload_(101) // same as asterisk
{
    memPool_.reset(pj_pool_create(&getSIPVoIPLink()->getCachingPool()->factory,
                                  id.c_str(), POOL_INITIAL_SIZE,
                                  POOL_INCREMENT_SIZE, NULL));
    const size_t suites_size = RING_ARRAYSIZE(CryptoSuites);
    std::vector<CryptoSuiteDefinition> localCaps(suites_size);
    std::copy(CryptoSuites, CryptoSuites + suites_size, localCaps.begin());
    sdesNego_ = {localCaps};
    if (not memPool_)
        throw std::runtime_error("pj_pool_create() failed");
}

Sdp::~Sdp()
{
    SIPAccount::releasePort(localAudioDataPort_);
#ifdef RING_VIDEO
    SIPAccount::releasePort(localVideoDataPort_);
#endif
}

static bool
hasPayload(const std::vector<std::shared_ptr<SystemAudioCodecInfo>> &codecs, int pt)
{
    for (const auto &i : codecs)
        if (i and i->payloadType_ == pt)
            return true;
    return false;
}

static bool
hasCodec(const std::vector<std::string> &codecs, const std::string &codec)
{
    return std::find(codecs.begin(), codecs.end(), codec) != codecs.end();
}

static std::string
rtpmapToString(pjmedia_sdp_rtpmap *rtpmap)
{
    std::ostringstream os;
    const std::string enc(rtpmap->enc_name.ptr, rtpmap->enc_name.slen);
    const std::string param(rtpmap->param.ptr, rtpmap->param.slen);
    os << enc << "/" << rtpmap->clock_rate;
    if (not param.empty())
        os << "/" << param;
    return os.str();
}

std::shared_ptr<SystemCodecInfo>
findCodecByName(const std::string &codec)
{
    // try finding by name
    return getSystemCodecContainer()->searchCodecByName(codec);
}

static void
randomFill(std::vector<uint8_t>& dest)
{
    std::uniform_int_distribution<uint8_t> rand_byte(0, 255);
    std::random_device rdev;
    std::generate(dest.begin(), dest.end(), std::bind(rand_byte, std::ref(rdev)));
}


void Sdp::setActiveLocalSdpSession(const pjmedia_sdp_session *sdp)
{
    activeLocalSession_ = (pjmedia_sdp_session*) sdp;

    sessionAudioMediaLocal_.clear();

    for (unsigned i = 0; i < activeLocalSession_->media_count; ++i) {
        pjmedia_sdp_media *current = activeLocalSession_->media[i];

        for (unsigned fmt = 0; fmt < current->desc.fmt_count; ++fmt) {
            static const pj_str_t STR_RTPMAP = { (char*) "rtpmap", 6 };
            pjmedia_sdp_attr *rtpMapAttribute = pjmedia_sdp_media_find_attr(current, &STR_RTPMAP, &current->desc.fmt[fmt]);

            if (!rtpMapAttribute) {
                RING_ERR("Could not find rtpmap attribute");
                break;
            }

            pjmedia_sdp_rtpmap *rtpmap;
            pjmedia_sdp_attr_to_rtpmap(memPool_.get(), rtpMapAttribute, &rtpmap);

            if (!pj_stricmp2(&current->desc.media, "audio")) {
                const unsigned long pt = pj_strtoul(&current->desc.fmt[fmt]);
                if (pt != telephoneEventPayload_ and not hasPayload(sessionAudioMediaLocal_, pt)) {
                    auto codec = std::dynamic_pointer_cast<SystemAudioCodecInfo>(getSystemCodecContainer()->searchCodecByPayload(pt,MEDIA_AUDIO));
                    if (codec)
                        sessionAudioMediaLocal_.push_back(codec);
                    else {
                        codec = std::dynamic_pointer_cast<SystemAudioCodecInfo>(findCodecByName(rtpmapToString(rtpmap)));
                        if (codec)
                            sessionAudioMediaLocal_.push_back(codec);
                        else
                            RING_ERR("Could not get codec for name %.*s", rtpmap->enc_name.slen, rtpmap->enc_name.ptr);
                    }
                }else{
                    RING_ERR("Can not find codec matching payload %d", pt);
                }
            } else if (!pj_stricmp2(&current->desc.media, "video")) {
                const string codec(rtpmap->enc_name.ptr, rtpmap->enc_name.slen);
                if (not hasCodec(sessionVideoMedia_, codec))
                    sessionVideoMedia_.push_back(codec);
            }
        }
    }
}


void Sdp::setActiveRemoteSdpSession(const pjmedia_sdp_session *sdp)
{
    if (!sdp) {
        RING_ERR("Remote sdp is NULL");
        return;
    }

    activeRemoteSession_ = (pjmedia_sdp_session*) sdp;

    sessionAudioMediaRemote_.clear();

    bool parsedTelelphoneEvent = false;
    for (unsigned i = 0; i < sdp->media_count; i++) {
        pjmedia_sdp_media *r_media = sdp->media[i];
        if (!pj_stricmp2(&r_media->desc.media, "audio")) {

            if (not parsedTelelphoneEvent) {
                static const pj_str_t STR_TELEPHONE_EVENT = { (char*) "telephone-event", 15};
                pjmedia_sdp_attr *telephoneEvent = pjmedia_sdp_attr_find(r_media->attr_count, r_media->attr, &STR_TELEPHONE_EVENT, NULL);

                if (telephoneEvent != NULL) {
                    pjmedia_sdp_rtpmap *rtpmap;
                    pjmedia_sdp_attr_to_rtpmap(memPool_.get(), telephoneEvent, &rtpmap);
                    telephoneEventPayload_ = pj_strtoul(&rtpmap->pt);
                    parsedTelelphoneEvent = true;
                }
            }

            // add audio codecs from remote as needed
            for (unsigned fmt = 0; fmt < r_media->desc.fmt_count; ++fmt) {
                static const pj_str_t STR_RTPMAP = { (char*) "rtpmap", 6 };
                pjmedia_sdp_attr *rtpMapAttribute = pjmedia_sdp_media_find_attr(r_media, &STR_RTPMAP, &r_media->desc.fmt[fmt]);

                if (!rtpMapAttribute) {
                    RING_ERR("Could not find rtpmap attribute");
                    break;
                }

                pjmedia_sdp_rtpmap *rtpmap;
                pjmedia_sdp_attr_to_rtpmap(memPool_.get(), rtpMapAttribute, &rtpmap);

                const unsigned long pt = pj_strtoul(&r_media->desc.fmt[fmt]);
                if (pt != telephoneEventPayload_ and not hasPayload(sessionAudioMediaRemote_, pt)) {
                    auto codec = std::dynamic_pointer_cast<SystemAudioCodecInfo>(getSystemCodecContainer()->searchCodecByPayload(pt));
                    if (codec) {
                        RING_DBG("Adding codec with new payload type %d", pt);
                        sessionAudioMediaRemote_.push_back(codec);
                    } else {
                        // Search by codec name, clock rate and param (channel count)
                        codec = std::dynamic_pointer_cast<SystemAudioCodecInfo> (findCodecByName(rtpmapToString(rtpmap)));
                        if (codec)
                            sessionAudioMediaRemote_.push_back(codec);
                        else
                            RING_ERR("Could not get codec for name %.*s", rtpmap->enc_name.slen, rtpmap->enc_name.ptr);
                    }
                }else{
                    RING_ERR("Can not find codec matching payload %d", pt);
                }
            }
        }
    }
}

pjmedia_sdp_attr *
Sdp::generateSdesAttribute()
{
    static constexpr const unsigned cryptoSuite = 0;
    std::vector<uint8_t> keyAndSalt;
    keyAndSalt.resize(ring::CryptoSuites[cryptoSuite].masterKeyLength / 8 + ring::CryptoSuites[cryptoSuite].masterSaltLength / 8);
    // generate keys
    randomFill(keyAndSalt);

    std::string tag = "1";
    std::string crypto_attr = tag + " " + ring::CryptoSuites[cryptoSuite].name + " inline:" + base64::encode(keyAndSalt);
    RING_DBG("%s", crypto_attr.c_str());

    pj_str_t val = { (char*) crypto_attr.c_str(), static_cast<pj_ssize_t>(crypto_attr.size()) };
    return pjmedia_sdp_attr_create(memPool_.get(), "crypto", &val);
}

pjmedia_sdp_media *
Sdp::setMediaDescriptorLines(bool audio, sip_utils::KeyExchangeProtocol kx)
{
    pjmedia_sdp_media *med = PJ_POOL_ZALLOC_T(memPool_.get(), pjmedia_sdp_media);

    med->desc.media = audio ? pj_str((char*) "audio") : pj_str((char*) "video");
    med->desc.port_count = 1;
    med->desc.port = audio ? localAudioDataPort_ : localVideoDataPort_;
    // in case of sdes, media are tagged as "RTP/SAVP", RTP/AVP elsewhere
    med->desc.transport = pj_str(kx == sip_utils::KeyExchangeProtocol::NONE ? (char*) "RTP/AVP" : (char*) "RTP/SAVP");

    int dynamic_payload = 96;

    med->desc.fmt_count = audio ? audio_codec_list_.size() : video_codec_list_.size();
    for (unsigned i = 0; i < med->desc.fmt_count; ++i) {
        unsigned clock_rate;
        string enc_name;
        int payload;
        std::string channels;
        unsigned channels_int;

        if (audio) {
            auto codec = audio_codec_list_[i];
            payload = codec->payloadType_;
            enc_name = codec->name_;
            channels_int = codec->nbChannels_;
            channels = std::to_string(channels_int).c_str();
            // G722 requires G722/8000 media description even though it's @ 16000 Hz
            // See http://tools.ietf.org/html/rfc3551#section-4.5.2
            if (codec->avcodecId_ == AV_CODEC_ID_ADPCM_G722)
                clock_rate = 8000;
            else
                clock_rate = codec->sampleRate_;
        } else {
            // FIXME: get this key from header
            enc_name = video_codec_list_[i]->name_;
            clock_rate = 90000;
            payload = dynamic_payload;
        }

        std::ostringstream s;
        s << payload;
        pj_strdup2(memPool_.get(), &med->desc.fmt[i], s.str().c_str());

        // Add a rtpmap field for each codec
        // We could add one only for dynamic payloads because the codecs with static RTP payloads
        // are entirely defined in the RFC 3351, but if we want to add other attributes like an asymmetric
        // connection, the rtpmap attribute will be useful to specify for which codec it is applicable
        pjmedia_sdp_rtpmap rtpmap;

        rtpmap.pt = med->desc.fmt[i];
        rtpmap.enc_name = pj_str((char*) enc_name.c_str());
        rtpmap.clock_rate = clock_rate;
        if ( audio && (channels_int > 1 )) {
            rtpmap.param.ptr = (char *) channels.c_str();
            rtpmap.param.slen = strlen(channels.c_str()); // don't include NULL terminator
        } else {
            rtpmap.param.slen = 0;
        }

        pjmedia_sdp_attr *attr;
        pjmedia_sdp_rtpmap_to_attr(memPool_.get(), &rtpmap, &attr);

        med->attr[med->attr_count++] = attr;

#ifdef RING_VIDEO
        if (enc_name == "H264") {
            std::ostringstream os;
            // FIXME: this should not be hardcoded, it will determine what profile and level
            // our peer will send us
            std::string profileLevelID(video_codec_list_[i]->parameters_);
            if (profileLevelID.empty())
                profileLevelID = libav_utils::MAX_H264_PROFILE_LEVEL_ID;
            os << "fmtp:" << dynamic_payload << " " << profileLevelID;
            med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_.get(), os.str().c_str(), NULL);
        }
#endif
        if (not audio)
            dynamic_payload++;
    }

    if (audio) {
        setTelephoneEventRtpmap(med);
        addRTCPAttribute(med); // video has its own RTCP
    }

    med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_.get(), "sendrecv", NULL);

    if (kx == sip_utils::KeyExchangeProtocol::SDES) {
        if (pjmedia_sdp_media_add_attr(med, generateSdesAttribute()) != PJ_SUCCESS)
            SdpException("Could not add sdes attribute to media");
    } /* else if (kx == sip_utils::KeyExchangeProtocol::ZRTP) {
        if (!zrtpHelloHash_.empty())
            addZrtpAttribute(med, zrtpHelloHash_);
    } */

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
            RING_ERR("Could not validate SDP");
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

void Sdp::setLocalMediaVideoCapabilities(const vector<unsigned> &selectedCodecs)
{
    video_codec_list_.clear();
#ifdef RING_VIDEO
    for (const auto &i : selectedCodecs) {
        auto codec = std::dynamic_pointer_cast<SystemVideoCodecInfo>(getSystemCodecContainer()->searchCodecById(i));

        if (codec)
            video_codec_list_.push_back(codec);
        else
            RING_WARN("Couldn't find video codec");
    }
#else
    (void) selectedCodecs;
#endif
}

void Sdp::setLocalMediaAudioCapabilities(const vector<unsigned> &selectedCodecs)
{
    if (selectedCodecs.empty())
        RING_WARN("No selected codec while building local SDP offer");

    audio_codec_list_.clear();
    for (const auto &i : selectedCodecs) {
        auto codec = std::dynamic_pointer_cast<SystemAudioCodecInfo>(getSystemCodecContainer()->searchCodecById(i));

        if (codec)
            audio_codec_list_.push_back(codec);
        else
            RING_WARN("Couldn't find audio codec");
    }
}

static void
printSession(const pjmedia_sdp_session *session)
{
    char buffer[2048];
    size_t size = pjmedia_sdp_print(session, buffer, sizeof(buffer));
    string sessionStr(buffer, std::min(size, sizeof(buffer)));
    RING_DBG("%s", sessionStr.c_str());
}

int Sdp::createLocalSession(const vector<unsigned> &selectedAudioCodecs, const vector<unsigned> &selectedVideoCodecs, sip_utils::KeyExchangeProtocol security)
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
    localSession_->media[0] = setMediaDescriptorLines(audio, security);
    if (not selectedVideoCodecs.empty()) {
        localSession_->media[1] = setMediaDescriptorLines(!audio, security);
        ++localSession_->media_count;
    }

    RING_DBG("SDP: Local SDP Session:");
    printSession(localSession_);

    return pjmedia_sdp_validate(localSession_);
}

bool
Sdp::createOffer(const vector<unsigned> &selectedAudioCodecs,
                 const vector<unsigned> &selectedVideoCodecs, sip_utils::KeyExchangeProtocol security)
{
    if (createLocalSession(selectedAudioCodecs, selectedVideoCodecs, security) != PJ_SUCCESS) {
        RING_ERR("Failed to create initial offer");
        return false;
    }

    if (pjmedia_sdp_neg_create_w_local_offer(memPool_.get(), localSession_, &negotiator_) != PJ_SUCCESS) {
        RING_ERR("Failed to create an initial SDP negotiator");
        return false;
    }
    return true;
}

void Sdp::receiveOffer(const pjmedia_sdp_session* remote,
                       const vector<unsigned> &selectedAudioCodecs,
                       const vector<unsigned> &selectedVideoCodecs, sip_utils::KeyExchangeProtocol kx)
{
    if (!remote) {
        RING_ERR("Remote session is NULL");
        return;
    }

    RING_DBG("Remote SDP Session:");
    printSession(remote);

    if (!localSession_ and createLocalSession(selectedAudioCodecs, selectedVideoCodecs, kx) != PJ_SUCCESS) {
        RING_ERR("Failed to create initial offer");
        return;
    }

    remoteSession_ = pjmedia_sdp_session_clone(memPool_.get(), remote);

    if (pjmedia_sdp_neg_create_w_remote_offer(memPool_.get(), localSession_,
            remoteSession_, &negotiator_) != PJ_SUCCESS)
        RING_ERR("Failed to initialize negotiator");
}

void Sdp::startNegotiation()
{
    if (negotiator_ == NULL) {
        RING_ERR("Can't start negotiation with invalid negotiator");
        return;
    }

    const pjmedia_sdp_session *active_local;
    const pjmedia_sdp_session *active_remote;

    if (pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO) {
        RING_WARN("Negotiator not in right state for negotiation");
        return;
    }

    if (pjmedia_sdp_neg_negotiate(memPool_.get(), negotiator_, 0) != PJ_SUCCESS)
        return;

    if (pjmedia_sdp_neg_get_active_local(negotiator_, &active_local) != PJ_SUCCESS)
        RING_ERR("Could not retrieve local active session");
    else
        setActiveLocalSdpSession(active_local);

    if (pjmedia_sdp_neg_get_active_remote(negotiator_, &active_remote) != PJ_SUCCESS)
        RING_ERR("Could not retrieve remote active session");
    else
        setActiveRemoteSdpSession(active_remote);
}


std::string
Sdp::getFilteredSdp(const pjmedia_sdp_session* session, unsigned media_keep)
{
    static constexpr size_t BUF_SZ = 4096;
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> tmpPool_(
        pj_pool_create(&getSIPVoIPLink()->getCachingPool()->factory, "tmpSdp", BUF_SZ, BUF_SZ, nullptr),
        pj_pool_release);
    pjmedia_sdp_session *cloned = pjmedia_sdp_session_clone(tmpPool_.get(), session);
    if (!cloned) {
        RING_ERR("Could not clone SDP");
        return "";
    }

    // deactivate non-video media
    bool hasKeep = false;
    for (unsigned i = 0; i < cloned->media_count; i++)
        if (i != media_keep) {
            if (pjmedia_sdp_media_deactivate(tmpPool_.get(), cloned->media[i]) != PJ_SUCCESS)
                RING_ERR("Could not deactivate media");
        } else {
            hasKeep = true;
        }

    if (not hasKeep) {
        RING_DBG("No media to keep present in SDP");
        return "";
    }

    // Leaking medias will be dropped with tmpPool_
    for (unsigned i = 0; i < cloned->media_count; i++)
        if (cloned->media[i]->desc.port == 0) {
            std::move(cloned->media+i+1, cloned->media+cloned->media_count, cloned->media+i);
            cloned->media_count--;
            i--;
        }

    // we handle crypto ourselfs, don't tell libav about it
    for (unsigned i = 0; i < cloned->media_count; i++) {
        auto media = cloned->media[i];
        while (true) {
            auto attr = pjmedia_sdp_attr_find2(media->attr_count, media->attr, "crypto", nullptr);
            if (not attr)
                break;
            pjmedia_sdp_attr_remove(&media->attr_count, media->attr, attr);
        }
    }

    char buffer[BUF_SZ];
    size_t size = pjmedia_sdp_print(cloned, buffer, sizeof(buffer));
    string sessionStr(buffer, std::min(size, sizeof(buffer)));

    return sessionStr;
}


std::vector<MediaDescription>
Sdp::getMediaSlots(const pjmedia_sdp_session* session, bool remote) const
{
    static const pj_str_t STR_RTPMAP = { (char*) "rtpmap", 6 };

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
            RING_ERR("Could not find connecion information for media");
            continue;
        }
        descr.addr = std::string(conn->addr.ptr, conn->addr.slen);
        descr.addr.setPort(media->desc.port);

        descr.holding = pjmedia_sdp_attr_find2(media->attr_count, media->attr, "recvonly", nullptr)
                     || pjmedia_sdp_attr_find2(media->attr_count, media->attr, "inactive", nullptr);

        // get codecs infos
        for (unsigned j = 0; j<media->desc.fmt_count; j++) {
            const pjmedia_sdp_attr *rtpMapAttribute = pjmedia_sdp_media_find_attr(media, &STR_RTPMAP, &media->desc.fmt[j]);
            if (!rtpMapAttribute) {
                RING_ERR("Could not find rtpmap attribute");
                continue;
            }
            pjmedia_sdp_rtpmap rtpmap;
            if (pjmedia_sdp_attr_get_rtpmap(rtpMapAttribute, &rtpmap) != PJ_SUCCESS || rtpmap.enc_name.slen == 0) {
                RING_ERR("Could not find payload type %s in SDP", media->desc.fmt[j]);
                continue;
            }
            const std::string codec_raw(rtpmap.enc_name.ptr, rtpmap.enc_name.slen);
            descr.codec = getSystemCodecContainer()->searchCodecByName(codec_raw);
            if (not descr.codec) {
                RING_ERR("Could not find codec %s", codec_raw.c_str());
                continue;
            }
            descr.payload_type = std::string(rtpmap.pt.ptr, rtpmap.pt.slen);
            if (descr.type == MEDIA_AUDIO) {
                descr.audioformat.sample_rate = rtpmap.clock_rate;
                if (rtpmap.param.slen && rtpmap.param.ptr)
                    descr.audioformat.nb_channels = std::stoi(std::string{rtpmap.param.ptr, rtpmap.param.slen});
            } else {
                //descr.bitrate = getOutgoingVideoField(codec, "bitrate");
            }
            // for now, first codec only
            break;
        }

        if (remote) {
            descr.receiving_sdp = getFilteredSdp(session, i);
            RING_WARN("receiving_sdp : %s", descr.receiving_sdp.c_str());
        }

        // get crypto info
        std::vector<std::string> crypto;
        for (unsigned j = 0; j < media->attr_count; j++) {
            pjmedia_sdp_attr *attribute = media->attr[j];
            if (pj_stricmp2(&attribute->name, "crypto") == 0)
                crypto.emplace_back(attribute->value.ptr, attribute->value.slen);
        }
        descr.crypto = sdesNego_.negotiate(crypto);
    }
    return ret;
}

namespace
{
    vector<string> split(const string &s, char delim)
    {
        vector<string> elems;
        stringstream ss(s);
        string item;
        while(getline(ss, item, delim))
            elems.push_back(item);
        return elems;
    }
} // end anonymous namespace

string Sdp::getLineFromSession(const pjmedia_sdp_session *sess, const string &keyword) const
{
    char buffer[2048];
    int size = pjmedia_sdp_print(sess, buffer, sizeof buffer);
    string sdp(buffer, size);
    const vector<string> tokens(split(sdp, '\n'));
    for (const auto &item : tokens)
        if (item.find(keyword) != string::npos)
            return item;
    return "";
}

static vector<map<string, string> >::const_iterator
findCodecInList(const vector<map<string, string> > &codecs, const string &codec)
{
    for (vector<map<string, string> >::const_iterator i = codecs.begin(); i != codecs.end(); ++i) {
        map<string, string>::const_iterator name = i->find("name");
        if (name != i->end() and (codec == name->second))
            return i;
    }
    return codecs.end();
}

#if 0
std::string
Sdp::getOutgoingVideoField(const std::string &codec, const char *key) const
{
    const vector<map<string, string> >::const_iterator i = findCodecInList(video_codec_list_, codec);
    if (i != video_codec_list_.end()) {
        map<string, string>::const_iterator field = i->find(key);
        if (field != i->end())
            return field->second;
    }
    return "";
}
#endif

void
Sdp::getProfileLevelID(const pjmedia_sdp_session *session,
                       std::string &profile, int payload) const
{
    std::ostringstream os;
    os << "a=fmtp:" << payload;
    string fmtpLine(getLineFromSession(session, os.str()));
    const std::string needle("profile-level-id=");
    const size_t DIGITS_IN_PROFILE_LEVEL_ID = 6;
    const size_t needleLength = needle.size() + DIGITS_IN_PROFILE_LEVEL_ID;
    const size_t pos = fmtpLine.find(needle);
    if (pos != std::string::npos and fmtpLine.size() >= (pos + needleLength)) {
        profile = fmtpLine.substr(pos, needleLength);
        RING_DBG("Using %s", profile.c_str());
    }
}

void Sdp::addZrtpAttribute(pjmedia_sdp_media* media, std::string hash)
{
    /* Format: ":version value" */
    std::string val = "1.10 " + hash;
    pj_str_t value = { (char*)val.c_str(), static_cast<pj_ssize_t>(val.size()) };
    pjmedia_sdp_attr *attr = pjmedia_sdp_attr_create(memPool_.get(), "zrtp-hash", &value);
    if (pjmedia_sdp_media_add_attr(media, attr) != PJ_SUCCESS)
        throw SdpException("Could not add zrtp attribute to media");
}

void
Sdp::addIceCandidates(unsigned media_index, const std::vector<std::string>& cands)
{
    if (media_index >= localSession_->media_count) {
        RING_ERR("addIceCandidates failed: cannot access media#%u (may be deactivated)", media_index);
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
    auto session = remoteSession_ ? remoteSession_ : activeRemoteSession_;
    if (not session) {
        RING_ERR("getIceCandidates failed: no remote session");
        return {};
    }
    if (media_index >= session->media_count) {
        RING_ERR("getIceCandidates failed: cannot access media#%u (may be deactivated)", media_index);
        return {};
    }
    auto media = session->media[media_index];
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
    auto session = remoteSession_ ? remoteSession_ : activeRemoteSession_;
    assert(session);

    for (unsigned i=0; i < session->attr_count; i++) {
        pjmedia_sdp_attr *attribute = session->attr[i];
        if (pj_stricmp2(&attribute->name, "ice-ufrag") == 0)
            ice_attrs.ufrag.assign(attribute->value.ptr, attribute->value.slen);
        else if (pj_stricmp2(&attribute->name, "ice-pwd") == 0)
            ice_attrs.pwd.assign(attribute->value.ptr, attribute->value.slen);
    }
    return ice_attrs;
}

// Returns index of desired media attribute, or -1 if not found */
static int
getIndexOfAttribute(const pjmedia_sdp_session * const session, const char * const type)
{
    if (!session) {
        RING_ERR("Session is NULL when looking for \"%s\" attribute", type);
        return -1;
    }
    size_t i = 0;
    while (i < session->media_count and pj_stricmp2(&session->media[i]->desc.media, type) != 0)
        ++i;

    if (i == session->media_count)
        return -1;
    else
        return i;
}

void Sdp::addAttributeToLocalAudioMedia(const char *attr)
{
    const int i = getIndexOfAttribute(localSession_, "audio");
    if (i == -1)
        return;
    pjmedia_sdp_attr *attribute = pjmedia_sdp_attr_create(memPool_.get(), attr, NULL);
    pjmedia_sdp_media_add_attr(localSession_->media[i], attribute);
}

void Sdp::removeAttributeFromLocalAudioMedia(const char *attr)
{
    const int i = getIndexOfAttribute(localSession_, "audio");
    if (i == -1)
        return;
    pjmedia_sdp_media_remove_all_attr(localSession_->media[i], attr);
}

void Sdp::removeAttributeFromLocalVideoMedia(const char *attr)
{
    const int i = getIndexOfAttribute(localSession_, "video");
    if (i == -1)
        return;
    pjmedia_sdp_media_remove_all_attr(localSession_->media[i], attr);
}

void Sdp::addAttributeToLocalVideoMedia(const char *attr)
{
    const int i = getIndexOfAttribute(localSession_, "video");
    if (i == -1)
        return;
    pjmedia_sdp_attr *attribute = pjmedia_sdp_attr_create(memPool_.get(), attr, NULL);
    pjmedia_sdp_media_add_attr(localSession_->media[i], attribute);
}

} // namespace ring
