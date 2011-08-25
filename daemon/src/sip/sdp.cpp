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
#include "sdpmedia.h"
#include "global.h"
#include "manager.h"
#include "video/video_endpoint.h"

#define ZRTP_VERSION "1.10"

static const pj_str_t STR_AUDIO = { (char*) "audio", 5};
static const pj_str_t STR_VIDEO = { (char*) "video", 5};
static const pj_str_t STR_IN = { (char*) "IN", 2 };
static const pj_str_t STR_IP4 = { (char*) "IP4", 3};
static const pj_str_t STR_IP6 = { (char*) "IP6", 3};
static const pj_str_t STR_RTP_AVP = { (char*) "RTP/AVP", 7 };
static const pj_str_t STR_RTP_SAVP = { (char*) "RTP/SAVP", 8 };
static const pj_str_t STR_SDP_NAME = { (char*) "sflphone", 8 };
static const pj_str_t STR_SENDRECV = { (char*) "sendrecv", 8 };
static const pj_str_t STR_RTPMAP = { (char*) "rtpmap", 6 };
static const pj_str_t STR_CRYPTO = { (char*) "crypto", 6 };
static const pj_str_t STR_TELEPHONE_EVENT = { (char*) "telephone-event", 15};

namespace // anonymous
{

pjmedia_sdp_media *getRemoteSdpMediaFromOffer (const pjmedia_sdp_session* remote_sdp,
                                       const char *media_type)
{
    if (!remote_sdp)
        return NULL;

    for (unsigned i = 0; i < remote_sdp->media_count; ++i)
        if (!pj_stricmp2 (&remote_sdp->media[i]->desc.media, media_type))
            return remote_sdp->media[i];

    return NULL;
}
} // end of anon namespace

Sdp::Sdp (pj_pool_t *pool)
    : memPool_(pool)
	, negotiator_(NULL)
    , localSession_(NULL)
	, remoteSession_(NULL)
    , activeLocalSession_(NULL)
    , activeRemoteSession_(NULL)
    , localAudioMediaCap_(NULL)
	, localVideoMediaCap_(NULL)
    , sessionAudioCodec_(NULL)
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
    _debug ("SDP: Set active local SDP session");

    activeLocalSession_ = (pjmedia_sdp_session*) sdp;

    CodecsMap audio_codecs_list = Manager::instance().getAudioCodecFactory().getCodecsMap();

    for (unsigned i = 0; i < activeLocalSession_->media_count ; i++) {
        // Retrieve the media
        pjmedia_sdp_media *current = activeLocalSession_->media[i];
        std::string type(current->desc.media.ptr, current->desc.media.slen);
		// Retrieve the payload
		pjmedia_sdp_attr *attribute = pjmedia_sdp_media_find_attr(current, &STR_RTPMAP, NULL);
		if (!attribute)
			return;
		pjmedia_sdp_rtpmap *rtpmap;
		pjmedia_sdp_attr_to_rtpmap (memPool_, attribute, &rtpmap);

		if (type == "audio")
			sessionAudioCodec_ = (sfl::AudioCodec*)audio_codecs_list[(AudioCodecType)pj_strtoul (&rtpmap->pt)];
        else if (type == "video")
			sessionVideoCodec_ = std::string(rtpmap->enc_name.ptr, rtpmap->enc_name.slen);
    }
}

void Sdp::setActiveRemoteSdpSession (const pjmedia_sdp_session *sdp)
{
    _debug ("SDP: Set negotiated SDP");

    activeRemoteSession_ = (pjmedia_sdp_session*) sdp;

    getRemoteSdpTelephoneEventFromOffer(sdp);
}

sfl::AudioCodec* Sdp::getSessionAudioCodec (void)
{
    return sessionAudioCodec_;
}

const std::string &Sdp::getSessionVideoCodec (void)
{
    return sessionVideoCodec_;
}

void Sdp::setMediaDescriptorLine (sdpMedia *media)
{
    pjmedia_sdp_media* med = PJ_POOL_ZALLOC_T (memPool_, pjmedia_sdp_media);
    pjmedia_sdp_attr *attr;
    bool audio = media->get_media_type() == MIME_TYPE_AUDIO;

    // Get the right media format
    pj_strdup (memPool_, &med->desc.media, audio ? &STR_AUDIO : &STR_VIDEO);
    med->desc.port_count = 1;
    med->desc.port = media->port;

    // in case of sdes, media are tagged as "RTP/SAVP", RTP/AVP elsewhere
    pj_strdup (memPool_, &med->desc.transport, srtpCrypto_.empty() ? &STR_RTP_AVP : &STR_RTP_SAVP);

    // Media format ( RTP payload )
    std::vector<sfl::Codec*> audio_list;
    std::vector<std::string> video_list;

    int count;
    if (audio) {
    	audio_list = media->get_media_audio_codec_list();
    	count = audio_list.size();
    } else {
    	video_list = media->get_media_video_codec_list();
    	count = video_list.size();
    }
    int dynamic_payload = 96;

    med->desc.fmt_count = count;

    for (int i = 0; i < count; i++) {
        unsigned clock_rate;
        const char *enc_name;
        int payload;

        if (audio) {
            sfl::Codec *codec = audio_list[i];
            payload = codec->getPayloadType ();
            enc_name = codec->getMimeSubtype().c_str();
            // G722 require G722/8000 media description even if it is 16000 codec
            if (codec->getPayloadType () == 9)
                clock_rate = 8000;
            else
                clock_rate = codec->getClockRate();
        } else {
			enc_name = video_list[i].c_str();
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
        rtpmap.enc_name = pj_str ((char*)enc_name);
        rtpmap.clock_rate = clock_rate;
        rtpmap.param.ptr = ((char* const)"");
        rtpmap.param.slen = 0;

        pjmedia_sdp_rtpmap_to_attr (memPool_, &rtpmap, &attr);

        med->attr[med->attr_count++] = attr;
    }

    // Add the direction stream
    attr = (pjmedia_sdp_attr*) pj_pool_zalloc (memPool_, sizeof (pjmedia_sdp_attr));

    pj_strdup2 (memPool_, &attr->name, "sendrecv");

    med->attr[ med->attr_count++] = attr;

    if (!zrtpHelloHash_.empty())
        addZrtpAttribute (med, zrtpHelloHash_);

    if (audio)
        setTelephoneEventRtpmap(med);

	localSession_->media[localSession_->media_count++] = med;
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

    delete localVideoMediaCap_;
    localVideoMediaCap_ = new sdpMedia (MIME_TYPE_VIDEO);
    localVideoMediaCap_->port = getLocalPublishedVideoPort();

    const std::vector<std::string> &codecs_list = sfl_video::getVideoCodecList();
    unsigned i;
    for (i=0; i<videoCodecs.size(); i++) {
    	const std::string &codec = videoCodecs[i];
    	unsigned j;
        for (j=0; j<codecs_list.size(); j++) {
			if (codecs_list[j] == codec) {
	        	localVideoMediaCap_->add_codec (codec);
	        	break;
			}
        }
        if (j == codecs_list.size())
        	_warn ("SDP: Couldn't find video codec %s", codec.c_str());
    }
}

void Sdp::setLocalMediaCapabilities (CodecOrder selectedCodecs)
{
    _debug ("SDP: Fetch local media capabilities. Local extern audio port: %i" , getLocalPublishedAudioPort());

    delete localAudioMediaCap_;
    localAudioMediaCap_ = new sdpMedia (MIME_TYPE_AUDIO);
    localAudioMediaCap_->port = getLocalPublishedAudioPort();

    /* We retrieve the codecs selected by the user */
    CodecsMap codecs_list = Manager::instance().getAudioCodecFactory().getCodecsMap();

    if (selectedCodecs.empty())
        _warn("No selected codec while building local SDP offer");
    else {
        for (CodecOrder::const_iterator iter = selectedCodecs.begin(); iter != selectedCodecs.end(); ++iter) {
            CodecsMap::const_iterator map_iter = codecs_list.find (*iter);

            if (map_iter != codecs_list.end())
                localAudioMediaCap_->add_codec (map_iter->second);
            else
                _warn ("SDP: Couldn't find audio codec");
        }
    }
}

int Sdp::createLocalSession (CodecOrder selectedCodecs, const std::vector<std::string> &videoCodecs)
{
    _info ("SDP: Create local session");

    // Build local media capabilities
    setLocalMediaCapabilities (selectedCodecs);
    setLocalMediaVideoCapabilities (videoCodecs);

    // Reference: RFC 4566 [5]

    /* Create and initialize basic SDP session */
    localSession_ = PJ_POOL_ZALLOC_T (memPool_, pjmedia_sdp_session);
    localSession_->conn = PJ_POOL_ZALLOC_T (memPool_, pjmedia_sdp_conn);
    localSession_->media_count = 0;

    /* Initialize the fields of the struct */
    addProtocol();
    addOrigin();
    addSessionName();
    addConnectionInfo();
    addTiming();
	setMediaDescriptorLine (localAudioMediaCap_);
	setMediaDescriptorLine (localVideoMediaCap_);

    if (!srtpCrypto_.empty())
        addSdesAttribute (srtpCrypto_);

    char buffer[10000];
    int size = pjmedia_sdp_print(localSession_, buffer, sizeof buffer - 1);
    buffer[size] = '\0';
    _debug("SDP: Local SDP Session:\n%s", buffer);

    // Validate the sdp session
    return pjmedia_sdp_validate (localSession_);
}

int Sdp::createOffer (CodecOrder selectedCodecs, const std::vector<std::string> &videoCodecs)
{
    pj_status_t status;

    _info ("SDP: Create initial offer");

    // Build the SDP session descriptor
    status = createLocalSession (selectedCodecs, videoCodecs);
    if (status != PJ_SUCCESS) {
        _error ("SDP: Error: Failed to create initial offer");
        return status;
    }

    // Create the SDP negotiator_ instance with local offer
    status = pjmedia_sdp_neg_create_w_local_offer (memPool_, localSession_, &negotiator_);
    if (status != PJ_SUCCESS) {
        _error ("SDP: Error: Failed to create an initial SDP negotiator");
        return status;
    }

    pjmedia_sdp_neg_get_state (negotiator_);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    return PJ_SUCCESS;
}

int Sdp::receiveOffer (const pjmedia_sdp_session* remote, const CodecOrder &selectedCodecs, const std::vector<std::string> &videoCodecs)
{
    if (!remote)
        return !PJ_SUCCESS;

    _debug ("SDP: Receiving initial offer");

    pj_status_t status;

    char buffer[1000];
    int size = pjmedia_sdp_print(remote, buffer, sizeof buffer - 1);
    buffer[size] = '\0';
    _debug("SDP: Remote SDP Session:\n%s", buffer);

    // If called for the first time
    if (!localSession_) {
        // Build the local offer to respond
        status = createLocalSession (selectedCodecs, videoCodecs);
        if (status != PJ_SUCCESS) {
            _error ("SDP: Error: Failed to create initial offer");
            return status;
        }
    }

    remoteSession_ = pjmedia_sdp_session_clone (memPool_, remote);

    status = pjmedia_sdp_neg_create_w_remote_offer (memPool_, localSession_,
                                                    remoteSession_,
                                                    &negotiator_);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    return PJ_SUCCESS;
}

int Sdp::receivingAnswerAfterInitialOffer(const pjmedia_sdp_session* remote)
{
    pj_status_t status;

    if (pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER)
        _warn("SDP: Session not in a valid state for receiving answer");

    status = pjmedia_sdp_neg_set_remote_answer(memPool_, negotiator_, remote);

    if (status != PJ_SUCCESS)
        _warn("SDP: Error: Could not set SDP remote answer");

    if (pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO)
        _warn("SDP: Session not in a valid state after receiving answer");

    return status;
}

int Sdp::generateAnswerAfterInitialOffer(void)
{
    pj_status_t status;

    if (pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER)
        _warn("SDP: Session not in a valid state for generating answer");

    status = pjmedia_sdp_neg_set_local_answer (memPool_, negotiator_, localSession_);

    if (status != PJ_SUCCESS)
        _warn("SDP: Error: could not set SDP local answer");

    if (pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO)
        _warn("SDP: Session not in a valid state after generating answer");

    return status;
}

pj_status_t Sdp::startNegotiation()
{
    pj_status_t status;
    const pjmedia_sdp_session *active_local;
    const pjmedia_sdp_session *active_remote;

    _debug ("SDP: Start negotiation");

    if (negotiator_ == NULL)
        _error("SDP: Error: negotiator is NULL in SDP session");

    if (pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO)
        _warn("SDP: Warning: negotiator not in wright state for negotiation");

    status = pjmedia_sdp_neg_negotiate (memPool_, negotiator_, 0);
    if (status != PJ_SUCCESS)
        return status;

    status = pjmedia_sdp_neg_get_active_local(negotiator_, &active_local);
    if (status != PJ_SUCCESS)
        _error("SDP: Could not retrieve local active session");
    else
        setActiveLocalSdpSession(active_local);

    status = pjmedia_sdp_neg_get_active_remote(negotiator_, &active_remote);
    if (status != PJ_SUCCESS)
        _error("SDP: Could not retrieve remote active session");
    else
        setActiveRemoteSdpSession(active_remote);

    return status;
}

void Sdp::updateInternalState()
{
    // Populate internal field
    updateMediaTransportInfoFromRemoteSdp();
}

void Sdp::addProtocol ()
{
    localSession_->origin.version = 0;
}

void Sdp::addOrigin ()
{
    pj_time_val tv;
    pj_gettimeofday (&tv);

    localSession_->origin.user = pj_str (pj_gethostname()->ptr);
    // Use Network Time Protocol format timestamp to ensure uniqueness.
    localSession_->origin.id = tv.sec + 2208988800UL;
    // The type of network ( IN for INternet )
    localSession_->origin.net_type = STR_IN;
    // The type of address
    localSession_->origin.addr_type = STR_IP4;
    // The address of the machine from which the session was created
    localSession_->origin.addr = pj_str ( (char*) localIpAddr_.c_str());
}

void Sdp::addSessionName ()
{
    localSession_->name = STR_SDP_NAME;
}

void Sdp::addConnectionInfo ()
{
    localSession_->conn->net_type = localSession_->origin.net_type;
    localSession_->conn->addr_type = localSession_->origin.addr_type;
    localSession_->conn->addr = localSession_->origin.addr;
}


void Sdp::addTiming ()
{
    // RFC 3264: An offer/answer model session description protocol
    // As the session is created and destroyed through an external signaling mean (SIP), the line
    // should have a value of "0 0".

    localSession_->time.start = localSession_->time.stop = 0;
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

std::string Sdp::getLineFromLocalSDP(const std::string &keyword) const
{
    assert(activeLocalSession_);
    char buffer[2048];
    int size = pjmedia_sdp_print(activeLocalSession_, buffer, sizeof buffer);
    std::string sdp(buffer, size);
    const vector<string> tokens(split(sdp, '\n'));
    for (vector<string>::const_iterator iter = tokens.begin(); iter != tokens.end(); ++iter)
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
    ss << "o=- 0 0 " << STR_IN.ptr << " " << STR_IP4.ptr << " " << localIpAddr_ << std::endl;
    ss << "s=" << STR_SDP_NAME.ptr << std::endl;
    ss << "c=" << STR_IN.ptr << " " << STR_IP4.ptr << " " << remoteIpAddr_ << std::endl;
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
        // the attribute to add to sdp
        pjmedia_sdp_attr *attribute = (pjmedia_sdp_attr*) pj_pool_zalloc (memPool_, sizeof (pjmedia_sdp_attr));

        attribute->name = pj_strdup3 (memPool_, "crypto");

        pj_strdup2(memPool_, &attribute->value, (*iter).c_str());

        // add crypto attribute to media
        for (unsigned i = 0; i < localSession_->media_count; i++)
            if (pjmedia_sdp_media_add_attr (localSession_->media[i], attribute) != PJ_SUCCESS)
                throw SdpException ("Could not add sdes attribute to media");
    }
}


void Sdp::addZrtpAttribute (pjmedia_sdp_media* media, std::string hash)
{
    pjmedia_sdp_attr *attribute = (pjmedia_sdp_attr*) pj_pool_zalloc (memPool_, sizeof (pjmedia_sdp_attr));

    attribute->name = pj_strdup3 (memPool_, "zrtp-hash");

    char *tmp;
    if (asprintf(&tmp, ZRTP_VERSION" %s", hash.c_str()) != -1)
    {
		pj_strdup2(memPool_, &attribute->value, tmp);
		free(tmp);
		if (pjmedia_sdp_media_add_attr (media, attribute) != PJ_SUCCESS)
			throw SdpException ("Could not add zrtp attribute to media");
    }
}

Sdp::~Sdp()
{
    delete localAudioMediaCap_;
    delete localVideoMediaCap_;
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

void Sdp::updateMediaTransportInfoFromRemoteSdp ()
{

    _info ("SDP: Fetching media from sdp");

    remoteIpAddr_ = std::string (activeRemoteSession_->conn->addr.ptr, activeRemoteSession_->conn->addr.slen);

    pjmedia_sdp_media *r_media = getRemoteSdpMediaFromOffer(activeRemoteSession_, "audio");
    if (!r_media)
        _warn ("SDP: Error: no remote sdp audio media found in the remote offer");
    else
    	remoteAudioPort_ = r_media->desc.port;

    r_media = getRemoteSdpMediaFromOffer (activeRemoteSession_, "video");
    if (!r_media)
        _warn ("SDP: Error: no remote sdp video media found in the remote offer");
    else
    	remoteVideoPort_ = r_media->desc.port;
}

void Sdp::getRemoteSdpTelephoneEventFromOffer(const pjmedia_sdp_session *remote_sdp)
{
    if (!remote_sdp) {
        _error("Sdp: Error: Remote sdp is NULL while parsing telephone event attribute");
        return;
    }

    pjmedia_sdp_media *r_media = NULL;
    for(unsigned i = 0; !r_media && i < remote_sdp->media_count; i++)
        if (pj_stricmp2(&remote_sdp->media[i]->desc.media, "audio") == 0)
            r_media = remote_sdp->media[i];

    if (!r_media) {
        _error("Sdp: Error: Could not find dtmf event from remote sdp");
        return;
    }

    pjmedia_sdp_attr *attribute = pjmedia_sdp_attr_find(r_media->attr_count, r_media->attr, &STR_TELEPHONE_EVENT, NULL);

    if (attribute) {
        pjmedia_sdp_rtpmap *rtpmap;
        pjmedia_sdp_attr_to_rtpmap (memPool_, attribute, &rtpmap);
        telephoneEventPayload_ = pj_strtoul (&rtpmap->pt);
    }
}

void Sdp::getRemoteSdpCryptoFromOffer (const pjmedia_sdp_session* remote_sdp, CryptoOffer& crypto_offer)
{
    for (unsigned i = 0; i < remote_sdp->media_count; ++i)
    {
        pjmedia_sdp_media *media = remote_sdp->media[i];
        for (unsigned j = 0; j < media->attr_count; j++)
        {
            pjmedia_sdp_attr *attribute = media->attr[j];
            if (pj_stricmp2 (&attribute->name, "crypto") == 0)
            {
                std::string attr (attribute->value.ptr, attribute->value.slen);
                // @TODO our parser require the "a=crypto:" to be present
                crypto_offer.push_back (attr + "a=crypto:");
            }
        }
    }
}
