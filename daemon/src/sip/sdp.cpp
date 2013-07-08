/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sdp.h"
#include "logger.h"
#include "manager.h"

#include <algorithm>

//#ifdef HAVE_OPUS
#include "audio/codecs/opus.h"
//#endif

#ifdef SFL_VIDEO
#include "video/libav_utils.h"
#endif

using std::string;
using std::map;
using std::vector;
using std::stringstream;

Sdp::Sdp(pj_pool_t *pool)
    : memPool_(pool)
    , negotiator_(NULL)
    , localSession_(NULL)
    , remoteSession_(NULL)
    , activeLocalSession_(NULL)
    , activeRemoteSession_(NULL)
    , audio_codec_list_()
    , video_codec_list_()
    , sessionAudioMedia_()
    , sessionVideoMedia_()
    , localIpAddr_()
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
{}

namespace {
    bool hasPayload(const std::vector<sfl::AudioCodec*> &codecs, int pt)
    {
        for (std::vector<sfl::AudioCodec*>::const_iterator i = codecs.begin(); i != codecs.end(); ++i)
            if (*i and (*i)->getPayloadType() == pt)
                return true;
        return false;
    }

    bool hasCodec(const std::vector<std::string> &codecs, const std::string &codec)
    {
        return std::find(codecs.begin(), codecs.end(), codec) != codecs.end();
    }
}


void Sdp::setActiveLocalSdpSession(const pjmedia_sdp_session *sdp)
{
    activeLocalSession_ = (pjmedia_sdp_session*) sdp;

    for (unsigned i = 0; i < activeLocalSession_->media_count; ++i) {
        pjmedia_sdp_media *current = activeLocalSession_->media[i];

        for (unsigned fmt = 0; fmt < current->desc.fmt_count; ++fmt) {
            static const pj_str_t STR_RTPMAP = { (char*) "rtpmap", 6 };
            pjmedia_sdp_attr *rtpMapAttribute = pjmedia_sdp_media_find_attr(current, &STR_RTPMAP, &current->desc.fmt[fmt]);

            if (!rtpMapAttribute) {
                ERROR("Could not find rtpmap attribute");
                break;
            }

            pjmedia_sdp_rtpmap *rtpmap;
            pjmedia_sdp_attr_to_rtpmap(memPool_, rtpMapAttribute, &rtpmap);

            if (!pj_stricmp2(&current->desc.media, "audio")) {
                const unsigned long pt = pj_strtoul(&current->desc.fmt[fmt]);
                if (pt != telephoneEventPayload_ and not hasPayload(sessionAudioMedia_, pt)) {
                    sfl::AudioCodec *codec = Manager::instance().audioCodecFactory.getCodec(pt);
                    if (codec)
                        sessionAudioMedia_.push_back(codec);
                    else
                        ERROR("Could not get codec for payload type %lu", pt);
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
        ERROR("Remote sdp is NULL");
        return;
    }

    activeRemoteSession_ = (pjmedia_sdp_session*) sdp;

    bool parsedTelelphoneEvent = false;
    for (unsigned i = 0; i < sdp->media_count; i++) {
        pjmedia_sdp_media *r_media = sdp->media[i];
        if (!pj_stricmp2(&r_media->desc.media, "audio")) {

            if (not parsedTelelphoneEvent) {
                static const pj_str_t STR_TELEPHONE_EVENT = { (char*) "telephone-event", 15};
                pjmedia_sdp_attr *telephoneEvent = pjmedia_sdp_attr_find(r_media->attr_count, r_media->attr, &STR_TELEPHONE_EVENT, NULL);

                if (telephoneEvent != NULL) {
                    pjmedia_sdp_rtpmap *rtpmap;
                    pjmedia_sdp_attr_to_rtpmap(memPool_, telephoneEvent, &rtpmap);
                    telephoneEventPayload_ = pj_strtoul(&rtpmap->pt);
                    parsedTelelphoneEvent = true;
                }
            }

            // add audio codecs from remote as needed
            for (unsigned fmt = 0; fmt < r_media->desc.fmt_count; ++fmt) {

                const unsigned long pt = pj_strtoul(&r_media->desc.fmt[fmt]);
                if (pt != telephoneEventPayload_ and not hasPayload(sessionAudioMedia_, pt)) {
                    sfl::AudioCodec *codec = Manager::instance().audioCodecFactory.getCodec(pt);
                    if (codec) {
                        DEBUG("Adding codec with new payload type %d", pt);
                        sessionAudioMedia_.push_back(codec);
                    } else
                        DEBUG("Could not get codec for payload type %lu", pt);
                }
            }
        }
    }
    DEBUG("Ready to decode %u audio codecs", sessionAudioMedia_.size());
}

string Sdp::getSessionVideoCodec() const
{
    if (sessionVideoMedia_.empty()) {
        DEBUG("Session video media is empty");
        return "";
    }
    return sessionVideoMedia_[0];
}

string Sdp::getAudioCodecNames() const
{
    std::string result;
    char sep = ' ';
    for (std::vector<sfl::AudioCodec*>::const_iterator i = sessionAudioMedia_.begin();
         i != sessionAudioMedia_.end(); ++i) {
        if (i == sessionAudioMedia_.end() - 1)
            sep = '\0';
        if (*i)
            result += (*i)->getMimeSubtype() + sep;
    }
    return result;
}

void Sdp::getSessionAudioMedia(std::vector<sfl::AudioCodec*> &codecs) const
{
    codecs = sessionAudioMedia_;
}


pjmedia_sdp_media *
Sdp::setMediaDescriptorLines(bool audio)
{
    pjmedia_sdp_media *med = PJ_POOL_ZALLOC_T(memPool_, pjmedia_sdp_media);

    med->desc.media = audio ? pj_str((char*) "audio") : pj_str((char*) "video");
    med->desc.port_count = 1;
    med->desc.port = audio ? localAudioDataPort_ : localVideoDataPort_;
    // in case of sdes, media are tagged as "RTP/SAVP", RTP/AVP elsewhere
    med->desc.transport = pj_str(srtpCrypto_.empty() ? (char*) "RTP/AVP" : (char*) "RTP/SAVP");

    int dynamic_payload = 96;

    med->desc.fmt_count = audio ? audio_codec_list_.size() : video_codec_list_.size();

    for (unsigned i = 0; i < med->desc.fmt_count; ++i) {
        unsigned clock_rate;
        string enc_name;
        int payload;
        unsigned channels;

        if (audio) {
            sfl::AudioCodec *codec = audio_codec_list_[i];
            payload = codec->getPayloadType();
            enc_name = codec->getMimeSubtype();
            clock_rate = codec->getClockRate();
            channels = codec->getChannels();
            // G722 require G722/8000 media description even if it is 16000 codec
            if (codec->getPayloadType () == 9)
                clock_rate = 8000;
        } else {
            // FIXME: get this key from header
            enc_name = video_codec_list_[i]["name"];
            clock_rate = 90000;
            payload = dynamic_payload;
        }

        std::ostringstream s;
        s << payload;
        pj_strdup2(memPool_, &med->desc.fmt[i], s.str().c_str());

        // Add a rtpmap field for each codec
        // We could add one only for dynamic payloads because the codecs with static RTP payloads
        // are entirely defined in the RFC 3351, but if we want to add other attributes like an asymmetric
        // connection, the rtpmap attribute will be useful to specify for which codec it is applicable
        pjmedia_sdp_rtpmap rtpmap;

        rtpmap.pt = med->desc.fmt[i];
        rtpmap.enc_name = pj_str((char*) enc_name.c_str());
        rtpmap.clock_rate = clock_rate;

// #ifdef HAVE_OPUS
        // Opus sample rate is allways declared as 48000 and channel num is allways 2 in rtpmap as per
        // http://tools.ietf.org/html/draft-spittka-payload-rtp-opus-03#section-6.2
        if(payload == Opus::PAYLOAD_TYPE) {
            rtpmap.clock_rate = 48000;
            rtpmap.param.ptr = ((char* const)"2");
            rtpmap.param.slen = 1;
        } else
// #endif
        {
            rtpmap.param.ptr = ((char* const)"");
            rtpmap.param.slen = 0;
        }

        pjmedia_sdp_attr *attr;
        pjmedia_sdp_rtpmap_to_attr(memPool_, &rtpmap, &attr);

        med->attr[med->attr_count++] = attr;

// #ifdef HAVE_OPUS
        // Declare stereo support for opus
        if(payload == Opus::PAYLOAD_TYPE) {
            std::ostringstream os;
            os << "fmtp:" << payload << " stereo=1; sprop-stereo=" << (channels>1 ? 1 : 0);
            med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_, os.str().c_str(), NULL);
        }
// #endif
#ifdef SFL_VIDEO
        if (enc_name == "H264") {
            std::ostringstream os;
            // FIXME: this should not be hardcoded, it will determine what profile and level
            // our peer will send us
            std::string profileLevelID(video_codec_list_[i]["parameters"]);
            if (profileLevelID.empty())
                profileLevelID = libav_utils::MAX_H264_PROFILE_LEVEL_ID;
            os << "fmtp:" << dynamic_payload << " " << profileLevelID;
            med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_, os.str().c_str(), NULL);
        }
#endif
        if (not audio)
            dynamic_payload++;
    }

    med->attr[med->attr_count++] = pjmedia_sdp_attr_create(memPool_, "sendrecv", NULL);
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
    std::ostringstream os;
    os << localIpAddr_ << ":" << localAudioControlPort_;
    const std::string str(os.str());
    pj_str_t input_str = pj_str((char*) str.c_str());
    pj_sockaddr outputAddr;
    pj_status_t status = pj_sockaddr_parse(PJ_AF_UNSPEC, 0, &input_str, &outputAddr);
    if (status != PJ_SUCCESS) {
        ERROR("Could not parse address %s", str.c_str());
        return;
    }
    pjmedia_sdp_attr *attr = pjmedia_sdp_attr_create_rtcp(memPool_, &outputAddr);
    if (attr)
        pjmedia_sdp_attr_add(&med->attr_count, med->attr, attr);
}

void
Sdp::updatePorts(const std::vector<pj_sockaddr_in> &sockets)
{
    localAudioDataPort_     = pj_ntohs(sockets[0].sin_port);
    localAudioControlPort_  = pj_ntohs(sockets[1].sin_port);
    localVideoDataPort_     = pj_ntohs(sockets[2].sin_port);
    localVideoControlPort_  = pj_ntohs(sockets[3].sin_port);
}


void Sdp::setTelephoneEventRtpmap(pjmedia_sdp_media *med)
{
    std::ostringstream s;
    s << telephoneEventPayload_;
    ++med->desc.fmt_count;
    pj_strdup2(memPool_, &med->desc.fmt[med->desc.fmt_count - 1], s.str().c_str());

    pjmedia_sdp_attr *attr_rtpmap = static_cast<pjmedia_sdp_attr *>(pj_pool_zalloc(memPool_, sizeof(pjmedia_sdp_attr)));
    attr_rtpmap->name = pj_str((char *) "rtpmap");
    attr_rtpmap->value = pj_str((char *) "101 telephone-event/8000");

    med->attr[med->attr_count++] = attr_rtpmap;

    pjmedia_sdp_attr *attr_fmtp = static_cast<pjmedia_sdp_attr *>(pj_pool_zalloc(memPool_, sizeof(pjmedia_sdp_attr)));
    attr_fmtp->name = pj_str((char *) "fmtp");
    attr_fmtp->value = pj_str((char *) "101 0-15");

    med->attr[med->attr_count++] = attr_fmtp;
}

void Sdp::setLocalMediaVideoCapabilities(const vector<map<string, string> > &codecs)
{
    video_codec_list_.clear();
#ifdef SFL_VIDEO
    if (codecs.empty())
        WARN("No selected video codec while building local SDP offer");
    else
        video_codec_list_ = codecs;
#else
    (void) codecs;
#endif
}

void Sdp::setLocalMediaAudioCapabilities(const vector<int> &selectedCodecs)
{
    if (selectedCodecs.empty())
        WARN("No selected codec while building local SDP offer");

    audio_codec_list_.clear();
    for (vector<int>::const_iterator i = selectedCodecs.begin(); i != selectedCodecs.end(); ++i) {
        sfl::AudioCodec *codec = Manager::instance().audioCodecFactory.getCodec(*i);

        if (codec)
            audio_codec_list_.push_back(codec);
        else
            WARN("Couldn't find audio codec");
    }
}

namespace {
    void printSession(const pjmedia_sdp_session *session)
    {
        char buffer[2048];
        size_t size = pjmedia_sdp_print(session, buffer, sizeof(buffer));
        string sessionStr(buffer, std::min(size, sizeof(buffer)));
        DEBUG("%s", sessionStr.c_str());
    }
}

int Sdp::createLocalSession(const vector<int> &selectedAudioCodecs, const vector<map<string, string> > &selectedVideoCodecs)
{
    setLocalMediaAudioCapabilities(selectedAudioCodecs);
    setLocalMediaVideoCapabilities(selectedVideoCodecs);

    localSession_ = PJ_POOL_ZALLOC_T(memPool_, pjmedia_sdp_session);
    localSession_->conn = PJ_POOL_ZALLOC_T(memPool_, pjmedia_sdp_conn);

    /* Initialize the fields of the struct */
    localSession_->origin.version = 0;
    pj_time_val tv;
    pj_gettimeofday(&tv);

    localSession_->origin.user = pj_str(pj_gethostname()->ptr);
    // Use Network Time Protocol format timestamp to ensure uniqueness.
    localSession_->origin.id = tv.sec + 2208988800UL;
    localSession_->origin.net_type = pj_str((char*) "IN");
    localSession_->origin.addr_type = pj_str((char*) "IP4");
    localSession_->origin.addr = pj_str((char*) localIpAddr_.c_str());

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

    DEBUG("SDP: Local SDP Session:");
    printSession(localSession_);

    return pjmedia_sdp_validate(localSession_);
}

bool
Sdp::createOffer(const vector<int> &selectedCodecs,
                 const vector<map<string, string> > &videoCodecs)
{
    if (createLocalSession(selectedCodecs, videoCodecs) != PJ_SUCCESS) {
        ERROR("Failed to create initial offer");
        return false;
    }

    if (pjmedia_sdp_neg_create_w_local_offer(memPool_, localSession_, &negotiator_) != PJ_SUCCESS) {
        ERROR("Failed to create an initial SDP negotiator");
        return false;
    }
    return true;
}

void Sdp::receiveOffer(const pjmedia_sdp_session* remote,
                       const vector<int> &selectedCodecs,
                       const vector<map<string, string> > &videoCodecs)
{
    if (!remote) {
        ERROR("Remote session is NULL");
        return;
    }

    DEBUG("Remote SDP Session:");
    printSession(remote);

    if (!localSession_ and createLocalSession(selectedCodecs, videoCodecs) != PJ_SUCCESS) {
        ERROR("Failed to create initial offer");
        return;
    }

    remoteSession_ = pjmedia_sdp_session_clone(memPool_, remote);

    pjmedia_sdp_neg_create_w_remote_offer(memPool_, localSession_,
                                          remoteSession_, &negotiator_);
}

void Sdp::startNegotiation()
{
    if (negotiator_ == NULL) {
        ERROR("Can't start negotiation with invalid negotiator");
        return;
    }

    const pjmedia_sdp_session *active_local;
    const pjmedia_sdp_session *active_remote;

    if (pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO)
        WARN("Negotiator not in right state for negotiation");

    if (pjmedia_sdp_neg_negotiate(memPool_, negotiator_, 0) != PJ_SUCCESS)
        return;

    if (pjmedia_sdp_neg_get_active_local(negotiator_, &active_local) != PJ_SUCCESS)
        ERROR("Could not retrieve local active session");
    else
        setActiveLocalSdpSession(active_local);

    if (pjmedia_sdp_neg_get_active_remote(negotiator_, &active_remote) != PJ_SUCCESS)
        ERROR("Could not retrieve remote active session");
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
    for (vector<string>::const_iterator iter = tokens.begin(); iter != tokens.end(); ++iter)
        if ((*iter).find(keyword) != string::npos)
            return *iter;
    return "";
}

// FIXME:
// Here we filter out parts of the SDP that libavformat doesn't need to
// know about...we should probably give the video decoder thread the original
// SDP and deal with the streams properly at that level
string Sdp::getIncomingVideoDescription() const
{
    pjmedia_sdp_session *videoSession = pjmedia_sdp_session_clone(memPool_, activeLocalSession_);
    if (!videoSession) {
        ERROR("Could not clone SDP");
        return "";
    }

    // deactivate non-video media
    bool hasVideo = false;
    for (unsigned i = 0; i < videoSession->media_count; i++)
        if (pj_stricmp2(&videoSession->media[i]->desc.media, "video")) {
            if (pjmedia_sdp_media_deactivate(memPool_, videoSession->media[i]) != PJ_SUCCESS)
                ERROR("Could not deactivate media");
        } else {
            hasVideo = true;
        }

    if (not hasVideo) {
        DEBUG("No video present in active local SDP");
        return "";
    }

    char buffer[4096];
    size_t size = pjmedia_sdp_print(videoSession, buffer, sizeof(buffer));
    string sessionStr(buffer, std::min(size, sizeof(buffer)));

    // FIXME: find a way to get rid of the "m=audio..." line with PJSIP

    const size_t audioPos = sessionStr.find("m=audio");
    const size_t newline2 = sessionStr.find('\n', audioPos);
    const size_t newline1 = sessionStr.rfind('\n', audioPos);

    sessionStr.erase(newline1, newline2 - newline1);
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

namespace {
    vector<map<string, string> >::const_iterator
        findCodecInList(const vector<map<string, string> > &codecs, const string &codec)
        {
            for (vector<map<string, string> >::const_iterator i = codecs.begin(); i != codecs.end(); ++i) {
                map<string, string>::const_iterator name = i->find("name");
                if (name != i->end() and (codec == name->second))
                    return i;
            }
            return codecs.end();
        }
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
        DEBUG("Using %s", profile.c_str());
    }
}

void Sdp::addSdesAttribute(const vector<std::string>& crypto)
{
    for (vector<std::string>::const_iterator iter = crypto.begin();
            iter != crypto.end(); ++iter) {
        pj_str_t val = { (char*)(*iter).c_str(), static_cast<pj_ssize_t>((*iter).size()) };
        pjmedia_sdp_attr *attr = pjmedia_sdp_attr_create(memPool_, "crypto", &val);

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
    pjmedia_sdp_attr *attr = pjmedia_sdp_attr_create(memPool_, "zrtp-hash", &value);

    if (pjmedia_sdp_media_add_attr(media, attr) != PJ_SUCCESS)
        throw SdpException("Could not add zrtp attribute to media");
}

namespace {
    // Returns index of desired media attribute, or -1 if not found */
    int getIndexOfAttribute(const pjmedia_sdp_session * const session, const char * const type)
    {
        if (!session) {
            ERROR("Session is NULL when looking for \"%s\" attribute", type);
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
}

void Sdp::addAttributeToLocalAudioMedia(const char *attr)
{
    const int i = getIndexOfAttribute(localSession_, "audio");
    if (i == -1)
        return;
    pjmedia_sdp_attr *attribute = pjmedia_sdp_attr_create(memPool_, attr, NULL);
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
    pjmedia_sdp_attr *attribute = pjmedia_sdp_attr_create(memPool_, attr, NULL);
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
        ERROR("Could not get remote IP from SDP or SDP Media");
}


void Sdp::setMediaTransportInfoFromRemoteSdp()
{
    if (!activeRemoteSession_) {
        ERROR("Remote sdp is NULL while parsing media");
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
#ifdef SFL_VIDEO
    string codec(getOutgoingVideoCodec());
    if (not codec.empty()) {
        const string encoder(libav_utils::encodersMap()[codec]);
        if (encoder.empty()) {
            DEBUG("Couldn't find encoder for \"%s\"\n", codec.c_str());
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
#endif
    return false;
}
