/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
#include "manager.h"
#include "logger.h"
#include "libav_utils.h"

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
    , remoteIpAddr_()
    , localAudioDataPort_(0)
    , localAudioControlPort_(0)
    , localVideoDataPort_(0)
    , localVideoControlPort_(0)
    , remoteAudioPort_(0)
    , remoteVideoPort_(0)
    , zrtpHelloHash_()
    , srtpCrypto_()
    , telephoneEventPayload_(101) // same as asterisk
{
    memPool_.reset(pj_pool_create(&getSIPVoIPLink()->getCachingPool()->factory,
                                  id.c_str(), POOL_INITIAL_SIZE,
                                  POOL_INCREMENT_SIZE, NULL));
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
hasPayload(const std::vector<ring::MediaAudioCodec*> &codecs, int pt)
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

static MediaCodec *
findCodecByName(const std::string &codec)
{
    // try finding by name
    return getMediaCodecFactory()->searchCodecByName(codec);
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
                    auto codec = dynamic_cast<MediaAudioCodec*>(getMediaCodecFactory()->searchCodecByPayload(pt,MEDIA_AUDIO));
                    if (codec)
                        sessionAudioMediaLocal_.push_back(codec);
                    else {
                        codec = dynamic_cast<MediaAudioCodec*> (findCodecByName(rtpmapToString(rtpmap)));
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
                    auto codec = dynamic_cast<MediaAudioCodec*>(getMediaCodecFactory()->searchCodecByPayload(pt));
                    if (codec) {
                        RING_DBG("Adding codec with new payload type %d", pt);
                        sessionAudioMediaRemote_.push_back(codec);
                    } else {
                        // Search by codec name, clock rate and param (channel count)
                        codec = dynamic_cast<MediaAudioCodec*> (findCodecByName(rtpmapToString(rtpmap)));
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

string Sdp::getSessionVideoCodec() const
{
    if (sessionVideoMedia_.empty()) {
        RING_DBG("Session video media is empty");
        return "";
    }
    return sessionVideoMedia_[0];
}


std::vector<ring::MediaAudioCodec*>
Sdp::getSessionAudioMedia() const
{
    vector<ring::MediaAudioCodec*> codecs;

    // Common codecs first
    for (auto c : sessionAudioMediaLocal_) {
        if (std::find(sessionAudioMediaRemote_.begin(), sessionAudioMediaRemote_.end(), c) != sessionAudioMediaRemote_.end())
            codecs.push_back(c);
    }
    RING_DBG("%u common audio codecs", codecs.size());

    // Next, the other codecs we declared to be able to encode
    for (auto c : sessionAudioMediaLocal_) {
        if (std::find(codecs.begin(), codecs.end(), c) == codecs.end())
            codecs.push_back(c);
    }
    // Finally, the remote codecs so we can decode them
    for (auto c : sessionAudioMediaRemote_) {
        if (std::find(codecs.begin(), codecs.end(), c) == codecs.end())
            codecs.push_back(c);
    }
    RING_DBG("Ready to decode %u audio codecs", codecs.size());

    return codecs;
}


pjmedia_sdp_media *
Sdp::setMediaDescriptorLines(bool audio)
{
    pjmedia_sdp_media *med = PJ_POOL_ZALLOC_T(memPool_.get(), pjmedia_sdp_media);

    med->desc.media = audio ? pj_str((char*) "audio") : pj_str((char*) "video");
    med->desc.port_count = 1;
    med->desc.port = audio ? localAudioDataPort_ : localVideoDataPort_;
    // in case of sdes, media are tagged as "RTP/SAVP", RTP/AVP elsewhere
    med->desc.transport = pj_str(srtpCrypto_.empty() ? (char*) "RTP/AVP" : (char*) "RTP/SAVP");

    int dynamic_payload = 96;

    med->desc.fmt_count = audio ? audio_codec_list_.size() : video_codec_list_.size();
    ring::MediaAudioCodec *codec = NULL;
    for (unsigned i = 0; i < med->desc.fmt_count; ++i) {
        unsigned clock_rate;
        string enc_name;
        int payload;
        std::string channels;


        if (audio) {
            codec = audio_codec_list_[i];
            payload = codec->payloadType_;
            enc_name = codec->name_;
            clock_rate = codec->sampleRate_;
            channels = std::to_string(codec->nbChannels_).c_str();
            // G722 requires G722/8000 media description even though it's @ 16000 Hz
            // See http://tools.ietf.org/html/rfc3551#section-4.5.2
            if (payload == 9)
                clock_rate = 8000;
        } else {
            // FIXME: get this key from header
            enc_name = video_codec_list_[i]["name"];
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
        rtpmap.param.ptr = (char *) channels.c_str();
        rtpmap.param.slen = strlen(channels.c_str()); // don't include NULL terminator

        pjmedia_sdp_attr *attr;
        pjmedia_sdp_rtpmap_to_attr(memPool_.get(), &rtpmap, &attr);

        med->attr[med->attr_count++] = attr;

#ifdef RING_VIDEO
        if (enc_name == "H264") {
            std::ostringstream os;
            // FIXME: this should not be hardcoded, it will determine what profile and level
            // our peer will send us
            std::string profileLevelID(video_codec_list_[i]["parameters"]);
            if (profileLevelID.empty())
                profileLevelID = libav_utils::MAX_H264_PROFILE_LEVEL_ID;
            os << "fmtp:" << dynamic_payload << " " << profileLevelID;
            med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_.get(), os.str().c_str(), NULL);
        }
#endif
        if (not audio)
            dynamic_payload++;
    }

    med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_.get(), "sendrecv", NULL);
    if (!zrtpHelloHash_.empty())
        addZrtpAttribute(med, zrtpHelloHash_);

    if (audio) {
        setTelephoneEventRtpmap(med);
        addRTCPAttribute(med); // video has its own RTCP
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
            RING_ERR("Could not validate SDP");
    }
}

void
Sdp::setPublishedIP(const IpAddr& ip_addr)
{
    setPublishedIP(ip_addr, ip_addr.getFamily());
}

void
Sdp::updatePorts(const std::vector<pj_sockaddr> &sockets)
{
    localAudioDataPort_     = pj_sockaddr_get_port(&sockets[0]);
    localAudioControlPort_  = pj_sockaddr_get_port(&sockets[1]);
    if (sockets.size() > 3) {
        localVideoDataPort_     = pj_sockaddr_get_port(&sockets[2]);
        localVideoControlPort_  = pj_sockaddr_get_port(&sockets[3]);
    } else {
        localVideoDataPort_ = 0;
        localVideoControlPort_ = 0;
    }

    if (localSession_) {
        if (localSession_->media[0]) {
            localSession_->media[0]->desc.port = localAudioDataPort_;
            // update RTCP attribute
            if (pjmedia_sdp_media_remove_all_attr(localSession_->media[0], "rtcp"))
                addRTCPAttribute(localSession_->media[0]);
        }
        if (localSession_->media[1])
            localSession_->media[1]->desc.port = localVideoDataPort_;

        if (not pjmedia_sdp_validate(localSession_))
            RING_ERR("Could not validate SDP");
    }
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

void Sdp::setLocalMediaVideoCapabilities(const vector<map<string, string> > &codecs)
{
    video_codec_list_.clear();
#ifdef RING_VIDEO
    if (codecs.empty())
        RING_WARN("No selected video codec while building local SDP offer");
    else
        video_codec_list_ = codecs;
#else
    (void) codecs;
#endif
}

void Sdp::setLocalMediaAudioCapabilities(const vector<int> &selectedCodecs)
{
    if (selectedCodecs.empty())
        RING_WARN("No selected codec while building local SDP offer");

    audio_codec_list_.clear();
    for (const auto &i : selectedCodecs) {
        auto codec = dynamic_cast<MediaAudioCodec*>(getMediaCodecFactory()->searchCodecById(i));

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

int Sdp::createLocalSession(const vector<int> &selectedAudioCodecs, const vector<map<string, string> > &selectedVideoCodecs)
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

    localSession_->name = pj_str((char*) PACKAGE);

    localSession_->conn->net_type = localSession_->origin.net_type;
    localSession_->conn->addr_type = localSession_->origin.addr_type;
    localSession_->conn->addr = localSession_->origin.addr;

    // RFC 3264: An offer/answer model session description protocol
    // As the session is created and destroyed through an external signaling mean (SIP), the line
    // should have a value of "0 0".
    localSession_->time.start = 0;
    localSession_->time.stop = 0;

    // For DTMF RTP events
    const bool audio = true;
    localSession_->media_count = 1;
    localSession_->media[0] = setMediaDescriptorLines(audio);
    if (not selectedVideoCodecs.empty()) {
        localSession_->media[1] = setMediaDescriptorLines(!audio);
        ++localSession_->media_count;
    }

    if (!srtpCrypto_.empty())
        addSdesAttribute(srtpCrypto_);

    RING_DBG("SDP: Local SDP Session:");
    printSession(localSession_);

    return pjmedia_sdp_validate(localSession_);
}

bool
Sdp::createOffer(const vector<int> &selectedCodecs,
                 const vector<map<string, string> > &videoCodecs)
{
    if (createLocalSession(selectedCodecs, videoCodecs) != PJ_SUCCESS) {
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
                       const vector<int> &selectedCodecs,
                       const vector<map<string, string> > &videoCodecs)
{
    if (!remote) {
        RING_ERR("Remote session is NULL");
        return;
    }

    RING_DBG("Remote SDP Session:");
    printSession(remote);

    if (!localSession_ and createLocalSession(selectedCodecs, videoCodecs) != PJ_SUCCESS) {
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

// removes token from line starting with prefix
static void
remove_token(std::string& text, const char* prefix, const std::string& token)
{
    const auto line_pos = text.find(prefix);
    if (line_pos == std::string::npos)
        return;

    const auto token_pos = text.find(token, line_pos);
    if (token_pos != std::string::npos)
        text.erase(token_pos, token.length());
}

static void
remove_line_with_token(std::string& text, const std::string& token)
{
    const auto tokenPos = text.find(token);
    if (tokenPos == std::string::npos) {
        RING_ERR("Couldn't find %s", token.c_str());
        return;
    }

    const auto post = text.find('\n', tokenPos);
    const auto pre  = text.rfind('\n', tokenPos);
    if (post > pre)
        text.erase(pre, post - pre);
}

// FIXME:
// Here we filter out parts of the SDP that libavformat doesn't need to
// know about...we should probably give the video decoder thread the original
// SDP and deal with the streams properly at that level
string Sdp::getIncomingVideoDescription() const
{
    pjmedia_sdp_session *videoSession = pjmedia_sdp_session_clone(memPool_.get(), activeLocalSession_);
    if (!videoSession) {
        RING_ERR("Could not clone SDP");
        return "";
    }

    // deactivate non-video media
    bool hasVideo = false;
    for (unsigned i = 0; i < videoSession->media_count; i++)
        if (pj_stricmp2(&videoSession->media[i]->desc.media, "video")) {
            if (pjmedia_sdp_media_deactivate(memPool_.get(), videoSession->media[i]) != PJ_SUCCESS)
                RING_ERR("Could not deactivate media");
        } else {
            hasVideo = true;
        }

    if (not hasVideo) {
        RING_DBG("No video present in active local SDP");
        return "";
    }

    char buffer[4096];
    size_t size = pjmedia_sdp_print(videoSession, buffer, sizeof(buffer));
    string sessionStr(buffer, std::min(size, sizeof(buffer)));

    // FIXME: find a way to get rid of the "m=audio..." line with PJSIP
    remove_line_with_token(sessionStr, "m=audio");

    return sessionStr;
}

static std::vector<std::string>
get_payloads(const char* prefix, const std::string& sdp)
{
    const auto line_start = sdp.find(prefix);
    if (line_start == std::string::npos)
        return {};

    // XXX: SDP lines end with \r\n
    const auto line_end = sdp.find('\r', line_start);
    if (line_end == std::string::npos)
        return {};

    // payload types are after the profile
    const auto profile_pos = sdp.find("RTP/AVP ", line_start);
    if (profile_pos == std::string::npos)
        return {};

    std::stringstream ss;
    ss << sdp.substr(profile_pos, line_end - profile_pos);

    std::vector<std::string> result;
    std::string tmp;
    int p;

    while (std::getline(ss, tmp, ' '))
        if (std::stringstream(tmp) >> p)
            result.emplace_back(tmp);

    return result;
}

// FIXME:
// Here we filter out parts of the SDP that libavformat doesn't need to
// know about...we should probably give the audio decoder thread the original
// SDP and deal with the streams properly at that level
std::string Sdp::getIncomingAudioDescription() const
{
    pjmedia_sdp_session *audioSession = pjmedia_sdp_session_clone(memPool_.get(), activeLocalSession_);
    if (!audioSession) {
        RING_ERR("Could not clone SDP");
        return "";
    }

    // deactivate non-audio media
    bool hasAudio = false;
    for (unsigned i = 0; i < audioSession->media_count; i++)
        if (pj_stricmp2(&audioSession->media[i]->desc.media, "audio")) {
            if (pjmedia_sdp_media_deactivate(memPool_.get(), audioSession->media[i]) != PJ_SUCCESS)
                RING_ERR("Could not deactivate media");
        } else {
            hasAudio = true;
        }

    if (not hasAudio) {
        RING_DBG("No audio present in active local SDP");
        return "";
    }

    char buffer[4096];
    const size_t size = pjmedia_sdp_print(audioSession, buffer, sizeof(buffer));
    std::string sessionStr(buffer, std::min(size, sizeof(buffer)));

    // FIXME: find a way to get rid of the "m=video..." line with PJSIP
    remove_line_with_token(sessionStr, "m=video");

    // drop all but first audio payload
    // FIXME: technically we should be able to decode any payload previously
    // announced by checking the payload type on the fly and switching codecs
    // appropriately
    const auto payloads = get_payloads("m=audio", sessionStr);

    bool extra = false;
    for (const auto& p : payloads) {
        if (extra) {
            RING_WARN("Dropping payload %s", p.c_str());
            remove_token(sessionStr, "m=audio", " " + p);
            remove_line_with_token(sessionStr, "a=rtpmap:" + p);
            remove_line_with_token(sessionStr, "a=fmtp:" + p);
        }
        extra = true;
    }

    return sessionStr;
}

std::string Sdp::getOutgoingVideoCodec() const
{
    string str("a=rtpmap:");
    std::stringstream os;
    os << getOutgoingVideoPayload();
    str += os.str();
    string vCodecLine(getLineFromSession(activeRemoteSession_, str));
    char codec_buf[32];
    codec_buf[0] = '\0';
    sscanf(vCodecLine.c_str(), "a=rtpmap:%*d %31[^/]", codec_buf);
    return string(codec_buf);
}

// FIXME: merge these into a single parsing function, lot of repetition here
std::string Sdp::getOutgoingAudioCodec() const
{
    string str("a=rtpmap:");
    std::stringstream os;
    os << getOutgoingAudioPayload();
    str += os.str();
    string aCodecLine(getLineFromSession(activeRemoteSession_, str));
    char codec_buf[32];
    codec_buf[0] = '\0';
    sscanf(aCodecLine.c_str(), "a=rtpmap:%*d %31[^/]", codec_buf);
    return string(codec_buf);
}

std::string Sdp::getOutgoingAudioRate() const
{
    // e.g. opus/48000/2, g722/16000
    string str("a=rtpmap:");
    std::stringstream os;
    os << getOutgoingAudioPayload();
    str += os.str();
    string aCodecLine(getLineFromSession(activeRemoteSession_, str));
    const auto pos = aCodecLine.find_first_of("/");
    if (pos < aCodecLine.size() - 1) {
        const auto tmp = aCodecLine.substr(pos + 1);
        // strip channel if present
        const auto end = tmp.find_first_of("/");
        if (end != string::npos)
            return tmp.substr(0, end);
        else
            return tmp;
    } else {
        const char *DEFAULT_RATE = "8000";
        RING_ERR("No rate found in SDP, defaulting to %s", DEFAULT_RATE);
        return DEFAULT_RATE;
    }
}

std::string Sdp::getOutgoingAudioChannels() const
{
    // e.g. opus/48000/2, g722/16000
    string str("a=rtpmap:");
    std::stringstream os;
    os << getOutgoingAudioPayload();
    str += os.str();
    string aCodecLine(getLineFromSession(activeRemoteSession_, str));

    const auto nb_slashes = std::count(aCodecLine.begin(), aCodecLine.end(), '/');
    const auto pos = aCodecLine.find_last_of("/");
    if (nb_slashes > 1 and pos < aCodecLine.size() - 1) {
        return aCodecLine.substr(pos + 1);
    } else {
        const char *DEFAULT_CHANNELS = "1";
        RING_ERR("No channels found in SDP, defaulting to %s", DEFAULT_CHANNELS);
        return DEFAULT_CHANNELS;
    }
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

int
Sdp::getOutgoingVideoPayload() const
{
    string videoLine(getLineFromSession(activeRemoteSession_, "m=video"));
    int payload_num;
    if (sscanf(videoLine.c_str(), "m=video %*d %*s %d", &payload_num) != 1)
        payload_num = 0;
    return payload_num;
}

int
Sdp::getOutgoingAudioPayload() const
{
    string audioLine(getLineFromSession(activeRemoteSession_, "m=audio"));
    int payload_num;
    if (sscanf(audioLine.c_str(), "m=audio %*d %*s %d", &payload_num) != 1)
        payload_num = 0;
    return payload_num;
}

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

void Sdp::addSdesAttribute(const vector<std::string>& crypto)
{
    for (const auto &item : crypto) {
        pj_str_t val = { (char*) item.c_str(), static_cast<pj_ssize_t>(item.size()) };
        pjmedia_sdp_attr *attr = pjmedia_sdp_attr_create(memPool_.get(), "crypto", &val);

        for (unsigned i = 0; i < localSession_->media_count; i++)
            if (pjmedia_sdp_media_add_attr(localSession_->media[i], attr) != PJ_SUCCESS)
                throw SdpException("Could not add sdes attribute to media");
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


void
Sdp::updateRemoteIP(unsigned index)
{
    // Remote connection information may be in the SDP Session or in the media
    // for that session
    pjmedia_sdp_conn *conn = activeRemoteSession_->conn ? activeRemoteSession_->conn :
                             activeRemoteSession_->media[index]->conn ? activeRemoteSession_->media[index]->conn :
                             NULL;
    if (conn)
        remoteIpAddr_ = std::string(conn->addr.ptr, conn->addr.slen);
    else
        RING_ERR("Could not get remote IP from SDP or SDP Media");
}


void Sdp::setMediaTransportInfoFromRemoteSdp()
{
    if (!activeRemoteSession_) {
        RING_ERR("Remote sdp is NULL while parsing media");
        return;
    }

    for (unsigned i = 0; i < activeRemoteSession_->media_count; ++i) {
        if (pj_stricmp2(&activeRemoteSession_->media[i]->desc.media, "audio") == 0) {
            remoteAudioPort_ = activeRemoteSession_->media[i]->desc.port;
            updateRemoteIP(i);
        } else if (pj_stricmp2(&activeRemoteSession_->media[i]->desc.media, "video") == 0) {
            remoteVideoPort_ = activeRemoteSession_->media[i]->desc.port;
            updateRemoteIP(i);
        }
    }
}

void Sdp::getRemoteSdpCryptoFromOffer(const pjmedia_sdp_session* remote_sdp, CryptoOffer& crypto_offer)
{
    for (unsigned i = 0; i < remote_sdp->media_count; ++i) {
        pjmedia_sdp_media *media = remote_sdp->media[i];

        for (unsigned j = 0; j < media->attr_count; j++) {
            pjmedia_sdp_attr *attribute = media->attr[j];

            // @TODO our parser require the "a=crypto:" to be present
            if (pj_stricmp2(&attribute->name, "crypto") == 0)
                crypto_offer.push_back("a=crypto:" + std::string(attribute->value.ptr, attribute->value.slen));
        }
    }
}

bool Sdp::getOutgoingVideoSettings(map<string, string> &args) const
{
#ifdef RING_VIDEO
    string codec(getOutgoingVideoCodec());
    if (not codec.empty()) {
        const string encoder(libav_utils::encodersMap()[codec]);
        if (encoder.empty()) {
            RING_DBG("Couldn't find video encoder for \"%s\"\n", codec.c_str());
            return false;
        } else {
            args["codec"] = encoder;
            args["bitrate"] = getOutgoingVideoField(codec, "bitrate");
            const int payload = getOutgoingVideoPayload();
            std::ostringstream os;
            os << payload;
            args["payload_type"] = os.str();
            // override with profile-level-id from remote, if present
            getProfileLevelID(activeRemoteSession_, args["parameters"], payload);
        }
        return true;
    }
#else
    (void) args;
#endif
    return false;
}

bool Sdp::getOutgoingAudioSettings(map<string, string> &args) const
{
    string codec(getOutgoingAudioCodec());
    if (not codec.empty()) {
        MediaAudioCodec* mediaAudioCodec = dynamic_cast<MediaAudioCodec*>(getMediaCodecFactory()->searchCodecByName(codec, MEDIA_AUDIO));
        if ( ! mediaAudioCodec ) {
            RING_DBG("Couldn't find encoder for \"%s\"\n", codec.c_str());
            return false;
        } else {
            RING_DBG("CANDIDATE: %s",mediaAudioCodec->to_string().c_str());
            args["codec"] = mediaAudioCodec->name_;
            const int payload = mediaAudioCodec->payloadType_;
            std::ostringstream os;
            os << payload;
            args["payload_type"] = os.str();
        }

        const string rate(getOutgoingAudioRate());
        if (rate.empty()) {
            RING_DBG("Couldn't find rate for \"%s\"\n", codec.c_str());
            return false;
        } else {
            // G722 requires G722/8000 media description even though it's @ 16000 Hz
            // See http://tools.ietf.org/html/rfc3551#section-4.5.2
            if (codec == "G722")
                args["sample_rate"] = "16000";
            else
                args["sample_rate"] = rate;
        }

        const string channels(getOutgoingAudioChannels());
        if (channels.empty()) {
            RING_DBG("Couldn't find channels for \"%s\"\n", codec.c_str());
            return false;
        } else {
            args["channels"] = channels;
        }

        return true;
    }
    return false;
}

} // namespace ring
