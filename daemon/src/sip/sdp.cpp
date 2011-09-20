/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "global.h"
#include "manager.h"
#include "video/video_endpoint.h"
#include <cassert>

Sdp::Sdp (pj_pool_t *pool)
    : memPool_(pool)
	, negotiator_(NULL)
    , localSession_(NULL)
	, remoteSession_(NULL)
    , activeLocalSession_(NULL)
    , activeRemoteSession_(NULL)
    , localIpAddr_("")
	, remoteIpAddr_("")
    , localAudioPort_(0)
	, localVideoPort_(0)
	, remoteAudioPort_(0)
	, remoteVideoPort_(0)
	, zrtpHelloHash_("")
	, srtpCrypto_()
    , telephoneEventPayload_(101) // same as asterisk
{
}

void Sdp::setActiveLocalSdpSession (const pjmedia_sdp_session *sdp)
{
    activeLocalSession_ = (pjmedia_sdp_session*) sdp;
    if (activeLocalSession_->media_count < 1)
    	return;

    pjmedia_sdp_media *current = activeLocalSession_->media[0];
    std::string type(current->desc.media.ptr, current->desc.media.slen);

	for (unsigned j = 0; j < current->desc.fmt_count; j++) {
		static const pj_str_t STR_RTPMAP = { (char*) "rtpmap", 6 };
	    pjmedia_sdp_attr *attribute = pjmedia_sdp_media_find_attr(current, &STR_RTPMAP, NULL);
		if (!attribute)
			continue;

	    pjmedia_sdp_rtpmap *rtpmap;
		pjmedia_sdp_attr_to_rtpmap (memPool_, attribute, &rtpmap);

	    if (type == "audio") {
			sfl::Codec *codec = Manager::instance().audioCodecFactory.getCodec((int) pj_strtoul (&rtpmap->pt));
			if (codec)
				sessionAudioMedia_.push_back(codec);
	    } else if (type == "video") {
	    	sessionVideoMedia_.push_back(std::string(rtpmap->enc_name.ptr, rtpmap->enc_name.slen));
	    }
	}
}



void Sdp::setActiveRemoteSdpSession (const pjmedia_sdp_session *sdp)
{
    activeRemoteSession_ = (pjmedia_sdp_session*) sdp;

    if(!sdp) {
        _error("Sdp: Error: Remote sdp is NULL while parsing telephone event attribute");
        return;
    }

    for (unsigned i = 0; i < sdp->media_count; i++)
        if(pj_stricmp2(&sdp->media[i]->desc.media, "audio") == 0) {
            pjmedia_sdp_media *r_media = sdp->media[i];
            static const pj_str_t STR_TELEPHONE_EVENT = { (char*) "telephone-event", 15};
            pjmedia_sdp_attr *attribute = pjmedia_sdp_attr_find(r_media->attr_count, r_media->attr, &STR_TELEPHONE_EVENT, NULL);
            if (attribute != NULL) {
                pjmedia_sdp_rtpmap *rtpmap;
                pjmedia_sdp_attr_to_rtpmap (memPool_, attribute, &rtpmap);
                telephoneEventPayload_ = pj_strtoul (&rtpmap->pt);
            }
            return;
        }

	_error("Sdp: Error: Could not found dtmf event from remote sdp");
}

std::string Sdp::getSessionVideoCodec (void) const
{
    if (sessionVideoMedia_.size() < 1)
    	return "";
    return sessionVideoMedia_[0];
}

std::string Sdp::getAudioCodecName(void) const
{
	try {
		sfl::AudioCodec *codec = getSessionAudioMedia();
		return codec ? codec->getMimeSubtype() : "";
	} catch(...) {
		return "";
	}
}

sfl::AudioCodec* Sdp::getSessionAudioMedia (void) const
{
    if (sessionAudioMedia_.size() < 1)
        throw SdpException("No codec description for this media");

    return static_cast<sfl::AudioCodec *>(sessionAudioMedia_[0]);
}


pjmedia_sdp_media *Sdp::setMediaDescriptorLine(bool audio)
{
    pjmedia_sdp_media *med = PJ_POOL_ZALLOC_T (memPool_, pjmedia_sdp_media);

    med->desc.media = audio ? pj_str((char*)"audio") : pj_str((char*)"video");
    med->desc.port_count = 1;
    med->desc.port = audio ? localAudioPort_ : localVideoPort_;
    // in case of sdes, media are tagged as "RTP/SAVP", RTP/AVP elsewhere
    med->desc.transport = pj_str(srtpCrypto_.empty() ? (char*)"RTP/AVP" : (char*)"RTP/SAVP");

    int dynamic_payload = 96;

    med->desc.fmt_count = audio ? audio_codec_list_.size() : video_codec_list_.size();
    for (unsigned i=0; i<med->desc.fmt_count; i++) {
    	unsigned clock_rate;
    	std::string enc_name;
    	int payload;

    	if (audio) {
    		sfl::Codec *codec = audio_codec_list_[i];
    		payload = codec->getPayloadType();
    		enc_name = codec->getMimeSubtype();
			clock_rate = codec->getClockRate();
    		// G722 require G722/8000 media description even if it is 16000 codec
    		if (codec->getPayloadType () == 9)
    			clock_rate = 8000;
		} else {
    		enc_name = video_codec_list_[i];
    		clock_rate = 90000;
    		payload = dynamic_payload++;
    	}

    	std::ostringstream s;
    	s << payload;
    	pj_strdup2 (memPool_, &med->desc.fmt[i], s.str().c_str());

        // Add a rtpmap field for each codec
        // We could add one only for dynamic payloads because the codecs with static RTP payloads
        // are entirely defined in the RFC 3351, but if we want to add other attributes like an asymmetric
        // connection, the rtpmap attribute will be useful to specify for which codec it is applicable
        pjmedia_sdp_rtpmap rtpmap;
		rtpmap.pt = med->desc.fmt[i];
        rtpmap.enc_name = pj_str ((char*)enc_name.c_str());
        rtpmap.clock_rate = clock_rate;
        rtpmap.param.ptr = ((char* const)"");
        rtpmap.param.slen = 0;

        pjmedia_sdp_attr *attr;
        pjmedia_sdp_rtpmap_to_attr (memPool_, &rtpmap, &attr);

        med->attr[med->attr_count++] = attr;
    }

    med->attr[ med->attr_count++] = pjmedia_sdp_attr_create(memPool_, "sendrecv", NULL);
    if (!zrtpHelloHash_.empty())
        addZrtpAttribute (med, zrtpHelloHash_);

    if (audio)
        setTelephoneEventRtpmap(med);

    return med;
}

void Sdp::setTelephoneEventRtpmap(pjmedia_sdp_media *med)
{
    pjmedia_sdp_attr *attr_rtpmap = static_cast<pjmedia_sdp_attr *>(pj_pool_zalloc(memPool_, sizeof(pjmedia_sdp_attr)));
    attr_rtpmap->name = pj_str((char *) "rtpmap");
    attr_rtpmap->value = pj_str((char *) "101 telephone-event/8000");

    med->attr[med->attr_count++] = attr_rtpmap;

    pjmedia_sdp_attr *attr_fmtp = static_cast<pjmedia_sdp_attr *>(pj_pool_zalloc(memPool_, sizeof(pjmedia_sdp_attr)));
    attr_fmtp->name = pj_str((char *) "fmtp");
    attr_fmtp->value = pj_str((char *) "101 0-15");

    med->attr[med->attr_count++] = attr_fmtp;
}

void Sdp::setLocalMediaVideoCapabilities (const std::vector<std::string> &videoCodecs)
{
    if (videoCodecs.empty())
        throw SdpException ("No selected video codec while building local SDP offer");

    video_codec_list_.clear();
    const std::vector<std::string> &codecs_list = sfl_video::getVideoCodecList();
    for (unsigned i=0; i<videoCodecs.size(); i++) {
    	const std::string &codec = videoCodecs[i];
        for (unsigned j=0; j<codecs_list.size(); j++) {
			if (codecs_list[j] == codec) {
	        	video_codec_list_.push_back(codec);
	        	break;
			}
        }
    }
}

void Sdp::setLocalMediaCapabilities (const CodecOrder &selectedCodecs)
{

    if (selectedCodecs.size() == 0)
        _warn("No selected codec while building local SDP offer");

    audio_codec_list_.clear();
	for (CodecOrder::const_iterator iter = selectedCodecs.begin(); iter != selectedCodecs.end(); ++iter) {
		sfl::Codec *codec = Manager::instance().audioCodecFactory.getCodec(*iter);
		if (codec)
			audio_codec_list_.push_back(codec);
		else
			_warn ("SDP: Couldn't find audio codec");
	}
}

int Sdp::createLocalSession (const CodecOrder &selectedCodecs, const std::vector<std::string> &videoCodecs)
{
    setLocalMediaCapabilities (selectedCodecs);
    setLocalMediaVideoCapabilities (videoCodecs);

    localSession_ = PJ_POOL_ZALLOC_T(memPool_, pjmedia_sdp_session);
    localSession_->conn = PJ_POOL_ZALLOC_T(memPool_, pjmedia_sdp_conn);

    /* Initialize the fields of the struct */
    localSession_->origin.version = 0;
    pj_time_val tv;
    pj_gettimeofday (&tv);

    localSession_->origin.user = pj_str (pj_gethostname()->ptr);
    // Use Network Time Protocol format timestamp to ensure uniqueness.
    localSession_->origin.id = tv.sec + 2208988800UL;
    localSession_->origin.net_type = pj_str((char*)"IN");
    localSession_->origin.addr_type = pj_str((char*)"IP4");
    localSession_->origin.addr = pj_str((char*)localIpAddr_.c_str());

    localSession_->name = pj_str((char*)"sflphone");

    localSession_->conn->net_type = localSession_->origin.net_type;
    localSession_->conn->addr_type = localSession_->origin.addr_type;
    localSession_->conn->addr = localSession_->origin.addr;

    // RFC 3264: An offer/answer model session description protocol
    // As the session is created and destroyed through an external signaling mean (SIP), the line
    // should have a value of "0 0".
    localSession_->time.start = 0;
    localSession_->time.stop = 0;

    // For DTMF RTP events
    localSession_->media_count = 2;
    localSession_->media[0] = setMediaDescriptorLine(true);
	localSession_->media[1] = setMediaDescriptorLine(false);

    if (!srtpCrypto_.empty())
        addSdesAttribute (srtpCrypto_);

    char buffer[1000];
    pjmedia_sdp_print(localSession_, buffer, sizeof(buffer));
    _debug("SDP: Local SDP Session:\n%s", buffer);

    return pjmedia_sdp_validate (localSession_);
}

void Sdp::createOffer (const CodecOrder &selectedCodecs, const std::vector<std::string> &videoCodecs)
{
    if (createLocalSession (selectedCodecs, videoCodecs) != PJ_SUCCESS)
        _error ("SDP: Error: Failed to create initial offer");
    else if (pjmedia_sdp_neg_create_w_local_offer (memPool_, localSession_, &negotiator_) != PJ_SUCCESS)
        _error ("SDP: Error: Failed to create an initial SDP negotiator");
}

void Sdp::receiveOffer (const pjmedia_sdp_session* remote,
                       const CodecOrder &selectedCodecs,
                       const std::vector<std::string> &videoCodecs)
{
    assert(remote);

    char buffer[1000];
    pjmedia_sdp_print(remote, buffer, sizeof(buffer));

    if(!localSession_ && createLocalSession (selectedCodecs, videoCodecs) != PJ_SUCCESS) {
		_error ("SDP: Failed to create initial offer");
		return;
	}

    remoteSession_ = pjmedia_sdp_session_clone (memPool_, remote);

    pj_status_t status = pjmedia_sdp_neg_create_w_remote_offer (memPool_, localSession_,
            remoteSession_, &negotiator_);

    assert(status == PJ_SUCCESS);
}

void Sdp::receivingAnswerAfterInitialOffer(const pjmedia_sdp_session* remote)
{
    assert(pjmedia_sdp_neg_get_state(negotiator_) == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER);
    assert(pjmedia_sdp_neg_set_remote_answer(memPool_, negotiator_, remote) == PJ_SUCCESS);
    assert(pjmedia_sdp_neg_get_state(negotiator_) == PJMEDIA_SDP_NEG_STATE_WAIT_NEGO);
}

void Sdp::startNegotiation()
{
    const pjmedia_sdp_session *active_local;
    const pjmedia_sdp_session *active_remote;

    assert(negotiator_);

    if (pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO)
        _warn("SDP: Warning: negotiator not in right state for negotiation");

    if (pjmedia_sdp_neg_negotiate (memPool_, negotiator_, 0) != PJ_SUCCESS)
        return;

    if (pjmedia_sdp_neg_get_active_local(negotiator_, &active_local) != PJ_SUCCESS)
        _error("SDP: Could not retrieve local active session");
    else
        setActiveLocalSdpSession(active_local);

    if (pjmedia_sdp_neg_get_active_remote(negotiator_, &active_remote) != PJ_SUCCESS)
        _error("SDP: Could not retrieve remote active session");
    else
        setActiveRemoteSdpSession(active_remote);
}

namespace
{
    using std::string;
    using std::vector;
    using std::stringstream;

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

Sdp::~Sdp()
{
}

std::string Sdp::getLineFromLocalSDP(const std::string &keyword) const
{
    assert(activeLocalSession_);
    char buffer[2048];
    int size = pjmedia_sdp_print(activeLocalSession_, buffer, sizeof buffer);
    std::string sdp(buffer, size);
    const std::vector<std::string> tokens(split(sdp, '\n'));
    for (std::vector<std::string>::const_iterator iter = tokens.begin(); iter != tokens.end(); ++iter)
        if ((*iter).find(keyword) != string::npos)
            return *iter;
    return "";
}

std::vector<std::string> Sdp::getActiveVideoDescription() const
{
    std::stringstream ss;
    if (activeLocalSession_)
    {
        static const int SIZE = 2048;
        char buffer[SIZE];
        int size = pjmedia_sdp_print(activeLocalSession_, buffer, SIZE);
        std::string localStr(buffer, size);
        _debug("ACTIVE LOCAL SESSION LOOKS LIKE: %s", localStr.c_str());
    }
    ss << "v=0" << std::endl;
    ss << "o=- 0 0 IN IP4 " << localIpAddr_ << std::endl;
    ss << "s=sflphone" << std::endl;
    ss << "c=IN IP4 " << remoteIpAddr_ << std::endl;
    ss << "t=0 0" << std::endl;
    //ss << "b=AS:1000" << std::endl;

    std::string videoLine(getLineFromLocalSDP("m=video"));
    ss << videoLine << std::endl;

    int payload;
    if (sscanf(videoLine.c_str(), "m=video %*d %*s %d", &payload) != 1)
		payload = 0;

    std::ostringstream s;
    s << "a=rtpmap:";
    s << payload;

    std::string vCodecLine(getLineFromLocalSDP(s.str()));
    ss << vCodecLine << std::endl;

    char codec[32];
    codec[0] = '\0';
    sscanf(vCodecLine.c_str(), "a=rtpmap:%*d %31[^/]", codec);

    std::vector<std::string> v;

    unsigned videoIdx = 0;
    while (pj_stricmp2(&activeLocalSession_->media[videoIdx]->desc.media, "video") != 0)
        ++videoIdx;

    // get direction string
    static const pj_str_t DIRECTIONS[] = {{(char*)"sendrecv", 8},
        {(char*)"sendonly", 8}, {(char*)"recvonly", 8} ,
        {(char*)"inactive", 8}, {NULL, 0}};
    const pj_str_t *guess = DIRECTIONS;
    pjmedia_sdp_attr *direction = NULL;

    while (!direction and guess->ptr)
    {
        direction = pjmedia_sdp_media_find_attr(activeLocalSession_->media[videoIdx], guess, NULL);
        ++guess;
    }

    if (direction)
    {
        std::string dir_str("a=");
        dir_str += std::string(direction->name.ptr, direction->name.slen);
        ss << dir_str << std::endl;
    }

    v.push_back(ss.str());
    v.push_back(codec);
    return v;
}

void Sdp::addSdesAttribute (const std::vector<std::string>& crypto)
{
    for (std::vector<std::string>::const_iterator iter = crypto.begin();
            iter != crypto.end(); ++iter)
    {
    	pj_str_t val = { (char*) (*iter).c_str(), (*iter).size() };
        pjmedia_sdp_attr *attr = pjmedia_sdp_attr_create(memPool_, "crypto", &val);

        for (unsigned i = 0; i < localSession_->media_count; i++)
            if (pjmedia_sdp_media_add_attr (localSession_->media[i], attr) != PJ_SUCCESS)
                throw SdpException ("Could not add sdes attribute to media");
    }
}


void Sdp::addZrtpAttribute (pjmedia_sdp_media* media, std::string hash)
{
    /* Format: ":version value" */
	std::string val = "1.10 " + hash;
	pj_str_t value = { (char*)val.c_str(), val.size() };
    pjmedia_sdp_attr *attr = pjmedia_sdp_attr_create(memPool_, "zrtp-hash", &value);

    if (pjmedia_sdp_media_add_attr (media, attr) != PJ_SUCCESS)
        throw SdpException ("Could not add zrtp attribute to media");
}


void Sdp::addAttributeToLocalAudioMedia(const char *attr)
{
    int i = 0;
    while (pj_stricmp2(&localSession_->media[i]->desc.media, "audio") != 0)
        ++i;
    pjmedia_sdp_attr *attribute = pjmedia_sdp_attr_create (memPool_, attr, NULL);
    pjmedia_sdp_media_add_attr (localSession_->media[i], attribute);
}

void Sdp::removeAttributeFromLocalAudioMedia(const char *attr)
{
    int i = 0;
    while (pj_stricmp2(&localSession_->media[i]->desc.media, "audio") != 0)
        ++i;
    pjmedia_sdp_media_remove_all_attr (localSession_->media[i], attr);
}

void Sdp::removeAttributeFromLocalVideoMedia(const char *attr)
{
    int i = 0;
    while (pj_stricmp2(&localSession_->media[i]->desc.media, "video") != 0)
        ++i;
    pjmedia_sdp_media_remove_all_attr (localSession_->media[i], attr);
}

void Sdp::addAttributeToLocalVideoMedia(const char *attr)
{
    int i = 0;
    while (pj_stricmp2(&localSession_->media[i]->desc.media, "video") != 0)
        ++i;

    pjmedia_sdp_attr *attribute = pjmedia_sdp_attr_create (memPool_, attr, NULL);
    pjmedia_sdp_media_add_attr(localSession_->media[i], attribute);
}

void Sdp::setMediaTransportInfoFromRemoteSdp ()
{
    if (!activeRemoteSession_) {
        _error("Sdp: Error: Remote sdp is NULL while parsing media");
        return;
    }

    remoteIpAddr_ = std::string (activeRemoteSession_->conn->addr.ptr, activeRemoteSession_->conn->addr.slen);

    for (unsigned i = 0; i < activeRemoteSession_->media_count; ++i)
        if (pj_stricmp2 (&activeRemoteSession_->media[i]->desc.media, "audio") == 0)
            remoteAudioPort_ = activeRemoteSession_->media[i]->desc.port;
        else if (pj_stricmp2 (&activeRemoteSession_->media[i]->desc.media, "video") == 0)
            remoteVideoPort_ = activeRemoteSession_->media[i]->desc.port;
}

void Sdp::getRemoteSdpCryptoFromOffer (const pjmedia_sdp_session* remote_sdp, CryptoOffer& crypto_offer)
{
    CryptoOffer remoteOffer;

    for (unsigned i = 0; i < remote_sdp->media_count; ++i) {
    	pjmedia_sdp_media *media = remote_sdp->media[i];
        for (unsigned j = 0; j < media->attr_count; j++) {
        	pjmedia_sdp_attr *attribute = media->attr[j];

        	// @TODO our parser require the "a=crypto:" to be present
            if (pj_stricmp2 (&attribute->name, "crypto") == 0)
                crypto_offer.push_back ("a=crypto:" + std::string(attribute->value.ptr, attribute->value.slen));
        }
    }
}
