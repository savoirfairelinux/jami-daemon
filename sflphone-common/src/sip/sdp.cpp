/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
static const char* const ZRTP_VERSION = "1.10";

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
void getRemoteSdpMediaFromOffer (const pjmedia_sdp_session* remote_sdp,
                                       pjmedia_sdp_media** r_media,
                                       const std::string &media_type)
{
    int count;

    if (!remote_sdp)
        return;

    count = remote_sdp->media_count;
    *r_media =  NULL;


    for (int i = 0; i < count; ++i) {
        if (pj_stricmp2 (&remote_sdp->media[i]->desc.media, media_type.c_str()) == 0) {
            *r_media = remote_sdp->media[i];
            return;
        }
    }
}
}

Sdp::Sdp (pj_pool_t *pool)
    : memPool_(pool)
	, negotiator_(NULL)
    , localSession_(NULL)
	, remoteSession_(NULL)
    , activeLocalSession_(NULL)
    , activeRemoteSession_(NULL)
    , localAudioMediaCap_()
    , sessionAudioMedia_(0)
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

    int nb_media, nb_codecs;
    int port;
    pjmedia_sdp_media *current;
    sdpMedia *media = NULL;
    std::string type, dir;
    CodecsMap codecs_list;
    pjmedia_sdp_attr *attribute = NULL;
    pjmedia_sdp_rtpmap *rtpmap;

    _debug ("SDP: Set active local SDP session");

    activeLocalSession_ = (pjmedia_sdp_session*) sdp;

    codecs_list = Manager::instance().getAudioCodecFactory().getCodecsMap();

    // retrieve the media information
    nb_media = activeLocalSession_->media_count;

    for (int i = 0; i < nb_media ; i++) {
        // Retrieve the media
        current = activeLocalSession_->media[i];
        type = current->desc.media.ptr;
        port = current->desc.port;
        media = new sdpMedia (type, port);
        // Retrieve the payload
        nb_codecs = current->desc.fmt_count;  // Must be one

        for (int j = 0; j < nb_codecs; j++) {
            attribute = pjmedia_sdp_media_find_attr(current, &STR_RTPMAP, NULL);
            // pj_strtoul(attribute->pt)

            if (!attribute)
            {
                delete media;
                return;
            }

            pjmedia_sdp_attr_to_rtpmap (memPool_, attribute, &rtpmap);

            CodecsMap::iterator iter = codecs_list.find ( (AudioCodecType) pj_strtoul (&rtpmap->pt));

            if (iter == codecs_list.end())
            {
                delete media;
                return;
            }

            media->add_codec (iter->second);
        }

        sessionAudioMedia_.push_back (media);
    }
}

void Sdp::setActiveRemoteSdpSession (const pjmedia_sdp_session *sdp)
{
    _debug ("SDP: Set negotiated SDP");

    activeRemoteSession_ = (pjmedia_sdp_session*) sdp;

    getRemoteSdpTelephoneEventFromOffer(sdp);
}

bool Sdp::hasSessionMedia(void) const
{
    return not sessionAudioMedia_.empty();
}

sfl::AudioCodec* Sdp::getSessionMedia (void)
{

    int nbMedia;
    int nbCodec;
    sfl::Codec *codec = NULL;
    std::vector<sdpMedia *> mediaList;

    _debug ("SDP: Get session media");

    nbMedia = sessionAudioMedia_.size();

    if (nbMedia <= 0) {
        _error("SDP: Error: No media in session description");
        throw SdpException("No media description for this SDP");
    }

    nbCodec = sessionAudioMedia_[0]->get_media_codec_list().size();

    if (nbCodec <= 0) {
        _error("SDP: Error: No codec description for this media");
        throw SdpException("No codec description for this media");
    }

    codec = sessionAudioMedia_[0]->get_media_codec_list()[0];

    return static_cast<sfl::AudioCodec *>(codec);
}

namespace
{
std::string convertIntToString (int value)
{
    std::ostringstream result;
    result << value;
    return result.str();
}
} // end anonymous namespace


void Sdp::setMediaDescriptorLine (sdpMedia *media, pjmedia_sdp_media** p_med)
{

    pjmedia_sdp_media* med;
    pjmedia_sdp_rtpmap rtpmap;
    pjmedia_sdp_attr *attr;
    sfl::Codec *codec;
    int count, i;
    std::string tmp;

    med = PJ_POOL_ZALLOC_T (memPool_, pjmedia_sdp_media);

    // Get the right media format
    pj_strdup (memPool_, &med->desc.media,
               (media->get_media_type() == MIME_TYPE_AUDIO) ? &STR_AUDIO : &STR_VIDEO);
    med->desc.port_count = 1;
    med->desc.port = media->get_port();

    // in case of sdes, media are tagged as "RTP/SAVP", RTP/AVP elsewhere
    if (srtpCrypto_.empty()) {
        pj_strdup (memPool_, &med->desc.transport, &STR_RTP_AVP);
    } else {

        pj_strdup (memPool_, &med->desc.transport, &STR_RTP_SAVP);
    }

    // Media format ( RTP payload )
    count = media->get_media_codec_list().size();
    med->desc.fmt_count = count;

    // add the payload list

    for (i=0; i<count; i++) {
        codec = media->get_media_codec_list() [i];
        tmp = convertIntToString (codec->getPayloadType ());
        _debug ("%s", tmp.c_str());
        pj_strdup2 (memPool_, &med->desc.fmt[i], tmp.c_str());

        // Add a rtpmap field for each codec
        // We could add one only for dynamic payloads because the codecs with static RTP payloads
        // are entirely defined in the RFC 3351, but if we want to add other attributes like an asymmetric
        // connection, the rtpmap attribute will be useful to specify for which codec it is applicable
        rtpmap.pt = med->desc.fmt[i];
        rtpmap.enc_name = pj_str ( (char*) codec->getMimeSubtype().c_str());

        // G722 require G722/8000 media description even if it is 16000 codec
        if (codec->getPayloadType () == 9) {
            rtpmap.clock_rate = 8000;
        } else {
            rtpmap.clock_rate = codec->getClockRate();
        }

        rtpmap.param.slen = 0;

        pjmedia_sdp_rtpmap_to_attr (memPool_, &rtpmap, &attr);

        med->attr[med->attr_count++] = attr;
    }

    // Add the direction stream
    attr = (pjmedia_sdp_attr*) pj_pool_zalloc (memPool_, sizeof (pjmedia_sdp_attr));

    pj_strdup2 (memPool_, &attr->name, media->get_stream_direction_str().c_str());

    med->attr[ med->attr_count++] = attr;

    if (!zrtpHelloHash_.empty()) {
        addZrtpAttribute (med, zrtpHelloHash_);
    }

    setTelephoneEventRtpmap(med);

    *p_med = med;
}

void Sdp::setTelephoneEventRtpmap(pjmedia_sdp_media *med)
{
    pjmedia_sdp_attr *attr_rtpmap = NULL;
    pjmedia_sdp_attr *attr_fmtp = NULL;

    attr_rtpmap = static_cast<pjmedia_sdp_attr *>(pj_pool_zalloc(memPool_, sizeof(pjmedia_sdp_attr)));
    attr_rtpmap->name = pj_str((char *) "rtpmap");
    attr_rtpmap->value = pj_str((char *) "101 telephone-event/8000");

    med->attr[med->attr_count++] = attr_rtpmap;

    attr_fmtp = static_cast<pjmedia_sdp_attr *>(pj_pool_zalloc(memPool_, sizeof(pjmedia_sdp_attr)));
    attr_fmtp->name = pj_str((char *) "fmtp");
    attr_fmtp->value = pj_str((char *) "101 0-15");

    med->attr[med->attr_count++] = attr_fmtp;
}

void Sdp::setLocalMediaCapabilities (CodecOrder selectedCodecs)
{

    unsigned int i;
    sdpMedia *audio;
    CodecsMap codecs_list;
    CodecsMap::iterator iter;

    // Clean it first
    localAudioMediaCap_.clear();

    _debug ("SDP: Fetch local media capabilities. Local extern audio port: %i" , getLocalPublishedAudioPort());

    /* Only one audio media used right now */
    audio = new sdpMedia (MIME_TYPE_AUDIO);
    audio->set_port (getLocalPublishedAudioPort());

    /* We retrieve the codecs selected by the user */
    codecs_list = Manager::instance().getAudioCodecFactory().getCodecsMap();

    if (selectedCodecs.size() == 0) {
        throw SdpException ("No selected codec while building local SDP offer");
    }

    for (i=0; i<selectedCodecs.size(); i++) {
        iter=codecs_list.find (selectedCodecs[i]);

        if (iter!=codecs_list.end()) {
            audio->add_codec (iter->second);
        } else {
            _warn ("SDP: Couldn't find audio codec");
        }
    }

    localAudioMediaCap_.push_back (audio);
}

int Sdp::createLocalSession (CodecOrder selectedCodecs)
{
    char buffer[1000];

    _info ("SDP: Create local session");

    // Build local media capabilities
    setLocalMediaCapabilities (selectedCodecs);

    // Reference: RFC 4566 [5]

    /* Create and initialize basic SDP session */
    localSession_ = PJ_POOL_ZALLOC_T (memPool_, pjmedia_sdp_session);
    localSession_->conn = PJ_POOL_ZALLOC_T (memPool_, pjmedia_sdp_conn);

    /* Initialize the fields of the struct */
    addProtocol();
    addOrigin();
    addSessionName();
    addConnectionInfo();
    addTiming();
    addAudioMediaDescription();
    addVideoMediaDescription();

    if (!srtpCrypto_.empty()) {
        addSdesAttribute (srtpCrypto_);
    }

    memset(buffer, 0, 1000);
    pjmedia_sdp_print(localSession_, buffer, 1000);
    _debug("SDP: Local SDP Session:\n%s", buffer);

    // Validate the sdp session
    return pjmedia_sdp_validate (localSession_);

}

int Sdp::createOffer (CodecOrder selectedCodecs)
{
    pj_status_t status;
    pjmedia_sdp_neg_state state;

    _info ("SDP: Create initial offer");

    // Build the SDP session descriptor
    status = createLocalSession (selectedCodecs);
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

    state = pjmedia_sdp_neg_get_state (negotiator_);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    return PJ_SUCCESS;
}

int Sdp::receiveOffer (const pjmedia_sdp_session* remote, CodecOrder selectedCodecs)
{
	char buffer[1000];

    _debug ("SDP: Receiving initial offer");

    pj_status_t status;

    if (!remote) {
        return !PJ_SUCCESS;
    }

    memset(buffer, 0, 1000);
    pjmedia_sdp_print(remote, buffer, 1000);
    _debug("SDP: Remote SDP Session:\n%s", buffer);

    // If called for the first time
    if(localSession_ == NULL) {
        // Build the local offer to respond
        status = createLocalSession (selectedCodecs);
        if (status != PJ_SUCCESS) {
            _error ("SDP: Error: Failed to create initial offer");
            return status;
        }
    }

    remoteSession_ = pjmedia_sdp_session_clone (memPool_, remote);

    status = pjmedia_sdp_neg_create_w_remote_offer (memPool_, localSession_,
                                                    remoteSession_, &negotiator_);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    return PJ_SUCCESS;
}

int Sdp::receivingAnswerAfterInitialOffer(const pjmedia_sdp_session* remote)
{
	pj_status_t status;

	if(pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER) {
		_warn("SDP: Session not in a valid state for receiving answer");
	}

	status = pjmedia_sdp_neg_set_remote_answer(memPool_, negotiator_, remote);

	if(status != PJ_SUCCESS) {
		_warn("SDP: Error: Could not set SDP remote answer");
	}

	if(pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO) {
		_warn("SDP: Session not in a valid state after receiving answer");
	}

	return status;
}

int Sdp::generateAnswerAfterInitialOffer(void)
{
	pj_status_t status;

	if(pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER) {
		_warn("SDP: Session not in a valid state for generating answer");
	}

	status = pjmedia_sdp_neg_set_local_answer (memPool_, negotiator_, localSession_);

	if(status != PJ_SUCCESS) {
		_warn("SDP: Error: could not set SDP local answer");
	}

	if(pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO) {
		_warn("SDP: Session not in a valid state after generating answer");
	}

	return status;

}

pj_status_t Sdp::startNegotiation()
{
    pj_status_t status;
	const pjmedia_sdp_session *active_local;
	const pjmedia_sdp_session *active_remote;

    _debug ("SDP: Start negotiation");

    if(negotiator_ == NULL) {
    	_error("SDP: Error: negotiator is NULL in SDP session");
    }

    if(pjmedia_sdp_neg_get_state(negotiator_) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO) {
    	_warn("SDP: Warning: negotiator not in wright state for negotiation");
    }

    status = pjmedia_sdp_neg_negotiate (memPool_, negotiator_, 0);
    if(status != PJ_SUCCESS) {
    	return status;
    }

	status = pjmedia_sdp_neg_get_active_local(negotiator_, &active_local);
	if(status != PJ_SUCCESS) {
		_error("SDP: Could not retrieve local active session");
	}

	setActiveLocalSdpSession(active_local);

	status = pjmedia_sdp_neg_get_active_remote(negotiator_, &active_remote);
	if(status != PJ_SUCCESS) {
		_error("SDP: Could not retrieve remote active session");
	}

	setActiveRemoteSdpSession(active_remote);

    return status;
}

void Sdp::updateInternalState() {

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

void Sdp::addAudioMediaDescription()
{
    pjmedia_sdp_media* med;
    int nb_media, i;

    med = PJ_POOL_ZALLOC_T (memPool_, pjmedia_sdp_media);
    nb_media = localAudioMediaCap_.size();
    // For DTMF RTP events
    localSession_->media_count = nb_media;

    for (i=0; i<nb_media; i++) {
        setMediaDescriptorLine (localAudioMediaCap_[i], &med);
        localSession_->media[i] = med;
    }
}

namespace {
std::vector<std::string>
&split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while(getline(ss, item, delim))
        elems.push_back(item);

    return elems;
}


std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    return split(s, delim, elems);
}
} // end anonymous namespace

void Sdp::addAttributesFromVideoSDP(pjmedia_sdp_media* med)
{
    using std::string;
    using std::vector;

    assert(not videoSDP_.empty());
    const vector<string> tokens(split(videoSDP_, '\n'));
    for (vector<string>::const_iterator iter = tokens.begin(); iter != tokens.end(); ++iter)
    {
        // if it's an attribute and not the tool attribute:
        if ((*iter)[0] == 'a' and (*iter).find("tool") == string::npos)
        {
            size_t separator_pos = (*iter).find(":");
            pjmedia_sdp_attr *attr = static_cast<pjmedia_sdp_attr *>(pj_pool_zalloc(memPool_, sizeof(pjmedia_sdp_attr)));
            string name((*iter).substr(2, separator_pos - 2)); // skip a= 
            string val((*iter).substr(separator_pos + 1, (*iter).size() - separator_pos));
            pj_strdup2(memPool_, &attr->name, name.c_str());
            pj_strdup2(memPool_, &attr->value, val.c_str());

            med->attr[med->attr_count++] = attr;
        }
    }
}

void Sdp::addVideoMediaDescription()
{
    // adds the following to the SDP:
    // m=video 5000 RTP/AVP 96
    // a=rtpmap:96 H264/90000

    pjmedia_sdp_media* med = PJ_POOL_ZALLOC_T (memPool_, pjmedia_sdp_media);

    // Get the right media format
    pj_strdup (memPool_, &med->desc.media, &STR_VIDEO);
    med->desc.port_count = 1;
    med->desc.port = getLocalPublishedVideoPort();
    assert(med->desc.port != 0);
    pj_strdup (memPool_, &med->desc.transport, &STR_RTP_AVP);
    std::string tmp("96");
    _debug("%s", tmp.c_str());
    pj_strdup2 (memPool_, &med->desc.fmt[0], tmp.c_str());
    med->desc.fmt_count = 1;
    pjmedia_sdp_rtpmap rtpmap;
    rtpmap.pt = med->desc.fmt[0];
    rtpmap.enc_name = pj_str((char*) "H264");
    rtpmap.clock_rate = 90000;
    rtpmap.param.slen = 0;
    pjmedia_sdp_attr *attr;
    pjmedia_sdp_rtpmap_to_attr (memPool_, &rtpmap, &attr);
    med->attr[med->attr_count++] = attr;

    // add it to the end
    localSession_->media[localAudioMediaCap_.size()] = med;
    ++localSession_->media_count;
}

std::string Sdp::getActiveVideoDescription() const
{
    std::stringstream ss;
    if (activeLocalSession_)
    {
        static const int SIZE = 2048;
        char buffer[SIZE];
        pjmedia_sdp_print(activeLocalSession_, buffer, SIZE);
        _error("ACTIVE LOCAL SESSION LOOKS LIKE: %s", buffer);
    }
    // Tue Jul 19 13:27:59 EDT 2011:tmatth:FIXME: this is called too early
    ss << "v=0" << std::endl;
    ss << "o=- 0 0 " << STR_IN.ptr << " " << STR_IP4.ptr << " " << localIpAddr_ << std::endl;
    ss << "s=" << STR_SDP_NAME.ptr << std::endl;
    ss << "c=" << STR_IN.ptr << " " << STR_IP4.ptr << " " << remoteIpAddr_ << std::endl;
    ss << "t=0 0" << std::endl;
    ss << "a=tool:libavformat 53.2.0" << std::endl;
    ss << "m=" << STR_VIDEO.ptr << " " << getLocalPublishedVideoPort() << " " << STR_RTP_AVP.ptr << " 96" << std::endl;
    ss << "b=AS:1000" << std::endl;
    ss << "a=rtpmap:96 H264/90000" << std::endl;
    ss << "a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z0LAHtoCgPaEAAADAAQAAAMA8DxYuoA=,aM48gA==" << std::endl;

    _debug("Receiving SDP \n%s", ss.str().c_str());

    return ss.str();
}

void Sdp::addSdesAttribute (const std::vector<std::string>& crypto)
{
    // temporary buffer used to store crypto attribute
    char tempbuf[256];

    std::vector<std::string>::const_iterator iter = crypto.begin();

    while (iter != crypto.end()) {

        // the attribute to add to sdp
        pjmedia_sdp_attr *attribute = (pjmedia_sdp_attr*) pj_pool_zalloc (memPool_, sizeof (pjmedia_sdp_attr));

        attribute->name = pj_strdup3 (memPool_, "crypto");

        // _debug("crypto from sdp: %s", crypto.c_str());


        int len = pj_ansi_snprintf (tempbuf, sizeof (tempbuf),
                                    "%.*s", (int) (*iter).size(), (*iter).c_str());

        attribute->value.slen = len;
        attribute->value.ptr = (char*) pj_pool_alloc (memPool_, attribute->value.slen+1);
        pj_memcpy (attribute->value.ptr, tempbuf, attribute->value.slen+1);

        // get number of media for this SDP
        int media_count = localSession_->media_count;

        // add crypto attribute to media
        for (int i = 0; i < media_count; i++) {

            if (pjmedia_sdp_media_add_attr (localSession_->media[i], attribute) != PJ_SUCCESS) {
                // if(pjmedia_sdp_attr_add(&(_local_offer->attr_count), _local_offer->attr, attribute) != PJ_SUCCESS){
                throw SdpException ("Could not add sdes attribute to media");
            }
        }


        ++iter;
    }
}


void Sdp::addZrtpAttribute (pjmedia_sdp_media* media, std::string hash)
{
    pjmedia_sdp_attr *attribute;
    char tempbuf[256];
    int len;

    attribute = (pjmedia_sdp_attr*) pj_pool_zalloc (memPool_, sizeof (pjmedia_sdp_attr));

    attribute->name = pj_strdup3 (memPool_, "zrtp-hash");

    /* Format: ":version value" */
    len = pj_ansi_snprintf (tempbuf, sizeof (tempbuf),
                            "%.*s %.*s",
                            4,
                            ZRTP_VERSION,
                            (int) hash.size(),
                            hash.c_str());

    attribute->value.slen = len;
    attribute->value.ptr = (char*) pj_pool_alloc (memPool_, attribute->value.slen+1);
    pj_memcpy (attribute->value.ptr, tempbuf, attribute->value.slen+1);

    if (pjmedia_sdp_media_add_attr (media, attribute) != PJ_SUCCESS) {
        throw SdpException ("Could not add zrtp attribute to media");
    }
}

void Sdp::cleanSessionMedia()
{
    _info ("SDP: Clean session media");

    if (not sessionAudioMedia_.empty()) {

        std::vector<sdpMedia *>::iterator iter = sessionAudioMedia_.begin();
        sdpMedia *media;

        while (iter != sessionAudioMedia_.end()) {
            _debug ("delete media");
            media = *iter;
            delete media;
            ++iter;
        }

        sessionAudioMedia_.clear();
    }
}


void Sdp::cleanLocalMediaCapabilities()
{
    _info ("SDP: Clean local media capabilities");

    if (not localAudioMediaCap_.empty()) {

        std::vector<sdpMedia *>::iterator iter = localAudioMediaCap_.begin();
        sdpMedia *media;

        while (iter != localAudioMediaCap_.end()) {
            media = *iter;
            delete media;
            ++iter;
        }

        localAudioMediaCap_.clear();
    }
}

void Sdp::setPortToAllMedia (int port)
{
    setLocalPublishedAudioPort (port);

    int size = localAudioMediaCap_.size();

    for (int i = 0; i < size; i++)
        localAudioMediaCap_[i]->set_port (port);
}

void Sdp::addAttributeToLocalAudioMedia(std::string attr)
{
    pjmedia_sdp_attr *attribute;

    attribute = pjmedia_sdp_attr_create (memPool_, attr.c_str(), NULL);

	pjmedia_sdp_media_add_attr (localSession_->media[0], attribute);
}

void Sdp::removeAttributeFromLocalAudioMedia(std::string attr)
{
	pjmedia_sdp_media_remove_all_attr (localSession_->media[0], attr.c_str());
}

void Sdp::updateMediaTransportInfoFromRemoteSdp ()
{
    pjmedia_sdp_media *r_media = NULL;

    _info ("SDP: Fetching media from sdp");

    if (!activeRemoteSession_) {
    	_error("Sdp: Error: Remote sdp is NULL while parsing media");
        return;
    }

    std::string media_type = "audio";
    getRemoteSdpMediaFromOffer(activeRemoteSession_, &r_media, media_type);

    if (r_media == NULL) {
        _warn ("SDP: Error: no remote sdp audio media found in the remote offer");
        return;
    }
    else {
        _info ("SDP: Remote Audio Port from fetching SDP: %d", r_media->desc.port);
        remoteAudioPort_ = r_media->desc.port;
    }

    std::string remote_ip(activeRemoteSession_->conn->addr.ptr, activeRemoteSession_->conn->addr.slen);
    _info ("SDP: Remote IP from fetching SDP: %s",  remote_ip.c_str());
    remoteIpAddr_ = remote_ip;

    r_media = NULL;
    media_type = "video";
    getRemoteSdpMediaFromOffer (activeRemoteSession_, &r_media, media_type);
    if (r_media == NULL) {
        _warn ("SDP: Error: no remote sdp video media found in the remote offer");
        return;
    }
    else {
        _info ("SDP: Remote Video Port from fetching SDP: %d", r_media->desc.port);
        remoteVideoPort_ = r_media->desc.port;
    }
}


void Sdp::getRemoteSdpTelephoneEventFromOffer(const pjmedia_sdp_session *remote_sdp)
{
	int media_count, attr_count;
	pjmedia_sdp_media *r_media = NULL;
	pjmedia_sdp_attr *attribute;
	pjmedia_sdp_rtpmap *rtpmap;

	if(!remote_sdp) {
		_error("Sdp: Error: Remote sdp is NULL while parsing telephone event attribute");
		return;
	}

	media_count = remote_sdp->media_count;

	for(int i = 0; i < media_count; i++) {
		if(pj_stricmp2(&remote_sdp->media[i]->desc.media, "audio") == 0) {
			r_media = remote_sdp->media[i];
			break;
		}
	}

	if(r_media == NULL) {
		_error("Sdp: Error: Could not found dtmf event gfrom remote sdp");
		return;
	}

	attr_count = r_media->attr_count;

	attribute = pjmedia_sdp_attr_find(attr_count, r_media->attr, &STR_TELEPHONE_EVENT, NULL);

	if(attribute != NULL) {

	    pjmedia_sdp_attr_to_rtpmap (memPool_, attribute, &rtpmap);

	    telephoneEventPayload_ = pj_strtoul (&rtpmap->pt);
	}
}

void Sdp::getRemoteSdpCryptoFromOffer (const pjmedia_sdp_session* remote_sdp, CryptoOffer& crypto_offer)
{

    int i, j;
    int attr_count, media_count;
    pjmedia_sdp_attr *attribute;
    pjmedia_sdp_media *media;

    // get the number of media for this sdp session
    media_count = remote_sdp->media_count;

    CryptoOffer remoteOffer;

    // iterate over all media
    for (i = 0; i < media_count; ++i) {

        // get media
        media = remote_sdp->media[i];

        // get number of attribute for this memdia
        attr_count = media->attr_count;

        // iterate over all attribute for this media
        for (j = 0; j < attr_count; j++) {

            attribute = media->attr[j];

            // test if this attribute is a crypto
            if (pj_stricmp2 (&attribute->name, "crypto") == 0) {

                std::string attr (attribute->value.ptr, attribute->value.slen);

                // @TODO our parser require the "a=crypto:" to be present
                std::string full_attr = "a=crypto:";
                full_attr += attr;

                crypto_offer.push_back (full_attr);
            }
        }
    }
}

