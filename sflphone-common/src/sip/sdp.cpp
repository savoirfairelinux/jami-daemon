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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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


Sdp::Sdp (pj_pool_t *pool)
    : memPool (pool)
	, negociator (NULL)
    , localSession (NULL)
	, remoteSession(NULL)
    , activeLocalSession (NULL)
    , activeRemoteSession (NULL)
    , localAudioMediaCap()
    , sessionAudioMedia (0)
    , localIpAddr ("")
	, remoteIpAddr ("")
    , localAudioPort (0)
	, remoteAudioPort (0)
	, zrtpHelloHash ("")
	, srtpCrypto()
{
}

Sdp::~Sdp()
{
}

void Sdp::setActiveLocalSdpSession (const pjmedia_sdp_session *sdp)
{

    int nb_media, nb_codecs;
    int i,j, port;
    pjmedia_sdp_media *current;
    sdpMedia *media = NULL;
    std::string type, dir;
    CodecsMap codecs_list;
    pjmedia_sdp_attr *attribute = NULL;
    pjmedia_sdp_rtpmap *rtpmap;

    _debug ("SDP: Set active local SDP session");

    activeLocalSession = (pjmedia_sdp_session*) sdp;

    codecs_list = Manager::instance().getCodecDescriptorMap().getCodecsMap();

    // retrieve the media information
    nb_media = activeLocalSession->media_count;

    for (i=0; i<nb_media ; i++) {
        // Retrieve the media
        current = activeLocalSession->media[i];
        type = current->desc.media.ptr;
        port = current->desc.port;
        media = new sdpMedia (type, port);
        // Retrieve the payload
        nb_codecs = current->desc.fmt_count;  // Must be one

        for (j=0 ; j<nb_codecs ; j++) {
            attribute = pjmedia_sdp_media_find_attr (current, &STR_RTPMAP, NULL);
            // pj_strtoul(attribute->pt)

            if (!attribute)
                return;

            pjmedia_sdp_attr_to_rtpmap (memPool, attribute, &rtpmap);

            CodecsMap::iterator iter = codecs_list.find ( (AudioCodecType) pj_strtoul (&rtpmap->pt));

            if (iter==codecs_list.end())
                return;

            media->add_codec (iter->second);
        }

        sessionAudioMedia.push_back (media);
    }
}

void Sdp::setActiveRemoteSdpSession (const pjmedia_sdp_session *sdp)
{

    std::string type, dir;
    CodecsMap codecs_list;

    _debug ("SDP: Set negotiated SDP");

    activeRemoteSession = (pjmedia_sdp_session*) sdp;
}

AudioCodec* Sdp::getSessionMedia (void)
{

    int nb_media;
    int nb_codec;
    sfl::Codec *codec = NULL;
    std::vector<sdpMedia*> media_list;

    _debug ("SDP: Get session media");

    media_list = getSessionMediaList ();
    nb_media = media_list.size();

    if (nb_media > 0) {
        nb_codec = media_list[0]->get_media_codec_list().size();

        if (nb_codec > 0) {
            codec = media_list[0]->get_media_codec_list() [0];
        }
    }

    return static_cast<AudioCodec *>(codec);
}

void Sdp::setMediaDescriptorLine (sdpMedia *media, pjmedia_sdp_media** p_med)
{

    pjmedia_sdp_media* med;
    pjmedia_sdp_rtpmap rtpmap;
    pjmedia_sdp_attr *attr;
    sfl::Codec *codec;
    int count, i;
    std::string tmp;

    med = PJ_POOL_ZALLOC_T (memPool, pjmedia_sdp_media);

    // Get the right media format
    pj_strdup (memPool, &med->desc.media,
               (media->get_media_type() == MIME_TYPE_AUDIO) ? &STR_AUDIO : &STR_VIDEO);
    med->desc.port_count = 1;
    med->desc.port = media->get_port();

    // in case of sdes, media are tagged as "RTP/SAVP", RTP/AVP elsewhere
    if (srtpCrypto.empty()) {

        pj_strdup (memPool, &med->desc.transport, &STR_RTP_AVP);
    } else {

        pj_strdup (memPool, &med->desc.transport, &STR_RTP_SAVP);
    }

    // Media format ( RTP payload )
    count = media->get_media_codec_list().size();
    med->desc.fmt_count = count;

    // add the payload list

    for (i=0; i<count; i++) {
        codec = media->get_media_codec_list() [i];
        tmp = this->convertIntToString (codec->getPayloadType ());
        _debug ("%s", tmp.c_str());
        pj_strdup2 (memPool, &med->desc.fmt[i], tmp.c_str());

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

        pjmedia_sdp_rtpmap_to_attr (memPool, &rtpmap, &attr);

        med->attr[med->attr_count++] = attr;
    }

    // Add the direction stream
    attr = (pjmedia_sdp_attr*) pj_pool_zalloc (memPool, sizeof (pjmedia_sdp_attr));

    pj_strdup2 (memPool, &attr->name, media->get_stream_direction_str().c_str());

    med->attr[ med->attr_count++] = attr;

    if (!zrtpHelloHash.empty()) {
        try {
            addZrtpAttribute (med,zrtpHelloHash);
        } catch (...) {
            throw;
        }
    }

    *p_med = med;
}

void Sdp::setLocalMediaCapabilities (CodecOrder selectedCodecs)
{

    unsigned int i;
    sdpMedia *audio;
    CodecsMap codecs_list;
    CodecsMap::iterator iter;

    // Clean it first
    localAudioMediaCap.clear();

    _debug ("SDP: Fetch local media capabilities. Local extern audio port: %i" , getLocalPublishedAudioPort());

    /* Only one audio media used right now */
    audio = new sdpMedia (MIME_TYPE_AUDIO);
    audio->set_port (getLocalPublishedAudioPort());

    /* We retrieve the codecs selected by the user */
    codecs_list = Manager::instance().getCodecDescriptorMap().getCodecsMap();

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

    localAudioMediaCap.push_back (audio);
}

int Sdp::createLocalSession (CodecOrder selectedCodecs)
{
    char buffer[1000];

    _info ("SDP: Create local session");

    // Build local media capabilities
    setLocalMediaCapabilities (selectedCodecs);

    // Reference: RFC 4566 [5]

    /* Create and initialize basic SDP session */
    localSession = PJ_POOL_ZALLOC_T (memPool, pjmedia_sdp_session);
    localSession->conn = PJ_POOL_ZALLOC_T (memPool, pjmedia_sdp_conn);

    /* Initialize the fields of the struct */
    addProtocol();
    addOrigin();
    addSessionName();
    addConnectionInfo();
    addTiming();
    addMediaDescription();

    if (!srtpCrypto.empty()) {
        addSdesAttribute (srtpCrypto);
    }

    memset(buffer, 0, 1000);
    pjmedia_sdp_print(getLocalSdpSession(), buffer, 1000);
    _debug("SDP: Local SDP Session: %s\n\n", buffer);

    // Validate the sdp session
    return pjmedia_sdp_validate (localSession);

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

    // Create the SDP negociator instance with local offer
    status = pjmedia_sdp_neg_create_w_local_offer (memPool, getLocalSdpSession(), &negociator);
    if (status != PJ_SUCCESS) {
        _error ("SDP: Error: Failed to create an initial SDP negociator");
        return status;
    }

    state = pjmedia_sdp_neg_get_state (negociator);

    _debug("SDP: Negociator state %s\n", pjmedia_sdp_neg_state_str(state));

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    return PJ_SUCCESS;
}

int Sdp::recieveOffer (const pjmedia_sdp_session* remote, CodecOrder selectedCodecs)
{
	char buffer[1000];

    _debug ("SDP: Receiving initial offer");

    pj_status_t status;

    if (!remote) {
        return !PJ_SUCCESS;
    }

    memset(buffer, 0, 1000);
    pjmedia_sdp_print(remote, buffer, 1000);
    _debug("SDP: Remote SDP Session: %s\n\n", buffer);

    // If called for the first time
    if(localSession == NULL) {
        // Build the local offer to respond
        status = createLocalSession (selectedCodecs);
        if (status != PJ_SUCCESS) {
            _error ("SDP: Error: Failed to create initial offer");
            return status;
        }
    }

    remoteSession = pjmedia_sdp_session_clone (memPool, remote);

    status = pjmedia_sdp_neg_create_w_remote_offer (memPool, getLocalSdpSession(), getRemoteSdpSession(), &negociator);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    return PJ_SUCCESS;
}

int Sdp::receivingAnswerAfterInitialOffer(const pjmedia_sdp_session* remote)
{
	pj_status_t status;

	if(pjmedia_sdp_neg_get_state(negociator) != PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER) {
		_warn("SDP: Session not in a valid state for receiving answer");
	}

	status = pjmedia_sdp_neg_set_remote_answer(memPool, negociator, remote);

	if(status != PJ_SUCCESS) {
		_warn("SDP: Error: Could not set SDP remote answer");
	}

	if(pjmedia_sdp_neg_get_state(negociator) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO) {
		_warn("SDP: Session not in a valid state after receiving answer");
	}
	_debug("SDP: Negotiator state %s\n", pjmedia_sdp_neg_state_str(pjmedia_sdp_neg_get_state(negociator)));

	return status;
}

int Sdp::generateAnswerAfterInitialOffer(void)
{
	pj_status_t status;

	if(pjmedia_sdp_neg_get_state(negociator) != PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER) {
		_warn("SDP: Session not in a valid state for generating answer");
	}

	status = pjmedia_sdp_neg_set_local_answer (memPool, negociator, localSession);

	if(status != PJ_SUCCESS) {
		_warn("SDP: Error: could not set SDP local answer");
	}

	if(pjmedia_sdp_neg_get_state(negociator) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO) {
		_warn("SDP: Session not in a valid state after generating answer");
	}

	return status;

}

pj_status_t Sdp::startNegociation()
{
    pj_status_t status;
	const pjmedia_sdp_session *active_local;
	const pjmedia_sdp_session *active_remote;

    _debug ("SDP: Start negotiation");

    if(negociator == NULL) {
    	_error("SDP: Error: Negociator is NULL in SDP session");
    }

    if(pjmedia_sdp_neg_get_state(negociator) != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO) {
    	_warn("SDP: Warning: negociator not in wright state for negotiation");
    }

    status = pjmedia_sdp_neg_negotiate (memPool, negociator, 0);
    if(status != PJ_SUCCESS) {
    	return status;
    }

    _debug("SDP: Negotiator state %s\n", pjmedia_sdp_neg_state_str(pjmedia_sdp_neg_get_state(negociator)));

	status = pjmedia_sdp_neg_get_active_local(negociator, &active_local);
	if(status != PJ_SUCCESS) {
		_error("SDP: Could not retrieve local active session");
	}

	setActiveLocalSdpSession(active_local);

	status = pjmedia_sdp_neg_get_active_remote(negociator, &active_remote);
	if(status != PJ_SUCCESS) {
		_error("SDP: Could not retrieve remote active session");
	}

	setActiveRemoteSdpSession(active_remote);

    return status;
}

void Sdp::updateInternalState() {

	// Populate internal field
	setMediaTransportInfoFromRemoteSdp (activeRemoteSession);
}

void Sdp::addProtocol (void)
{

    this->localSession->origin.version = 0;
}

void Sdp::addOrigin (void)
{

    pj_time_val tv;
    pj_gettimeofday (&tv);

    this->localSession->origin.user = pj_str (pj_gethostname()->ptr);
    // Use Network Time Protocol format timestamp to ensure uniqueness.
    this->localSession->origin.id = tv.sec + 2208988800UL;
    // The type of network ( IN for INternet )
    this->localSession->origin.net_type = STR_IN;
    // The type of address
    this->localSession->origin.addr_type = STR_IP4;
    // The address of the machine from which the session was created
    this->localSession->origin.addr = pj_str ( (char*) localIpAddr.c_str());
}

void Sdp::addSessionName (void)
{

    this->localSession->name = STR_SDP_NAME;
}


void Sdp::addConnectionInfo (void)
{

    this->localSession->conn->net_type = localSession->origin.net_type;
    this->localSession->conn->addr_type = localSession->origin.addr_type;
    this->localSession->conn->addr = localSession->origin.addr;
}


void Sdp::addTiming (void)
{

    // RFC 3264: An offer/answer model session description protocol
    // As the session is created and destroyed through an external signaling mean (SIP), the line
    // should have a value of "0 0".

    this->localSession->time.start = this->localSession->time.stop = 0;
}

void Sdp::addAttributes()
{

    pjmedia_sdp_attr *a;
    this->localSession->attr_count = 1;
    a =  PJ_POOL_ZALLOC_T (memPool, pjmedia_sdp_attr);
    a->name=STR_SENDRECV;
    localSession->attr[0] = a;
}


void Sdp::addMediaDescription()
{
    pjmedia_sdp_media* med;
    int nb_media, i;

    med = PJ_POOL_ZALLOC_T (memPool, pjmedia_sdp_media);
    nb_media = getLocalMediaCap().size();
    this->localSession->media_count = nb_media;

    for (i=0; i<nb_media; i++) {
        setMediaDescriptorLine (getLocalMediaCap() [i], &med);
        this->localSession->media[i] = med;
    }
}

void Sdp::addSdesAttribute (std::vector<std::string>& crypto) throw (SdpException)
{

    // temporary buffer used to store crypto attribute
    char tempbuf[256];

    std::vector<std::string>::iterator iter = crypto.begin();

    while (iter != crypto.end()) {

        // the attribute to add to sdp
        pjmedia_sdp_attr *attribute = (pjmedia_sdp_attr*) pj_pool_zalloc (memPool, sizeof (pjmedia_sdp_attr));

        attribute->name = pj_strdup3 (memPool, "crypto");

        // _debug("crypto from sdp: %s", crypto.c_str());


        int len = pj_ansi_snprintf (tempbuf, sizeof (tempbuf),
                                    "%.*s", (int) (*iter).size(), (*iter).c_str());

        attribute->value.slen = len;
        attribute->value.ptr = (char*) pj_pool_alloc (memPool, attribute->value.slen+1);
        pj_memcpy (attribute->value.ptr, tempbuf, attribute->value.slen+1);

        // get number of media for this SDP
        int media_count = localSession->media_count;

        // add crypto attribute to media
        for (int i = 0; i < media_count; i++) {

            if (pjmedia_sdp_media_add_attr (localSession->media[i], attribute) != PJ_SUCCESS) {
                // if(pjmedia_sdp_attr_add(&(_local_offer->attr_count), _local_offer->attr, attribute) != PJ_SUCCESS){
                throw SdpException ("Could not add sdes attribute to media");
            }
        }


        iter++;
    }
}


void Sdp::addZrtpAttribute (pjmedia_sdp_media* media, std::string hash) throw (SdpException)
{
    pjmedia_sdp_attr *attribute;
    char tempbuf[256];
    int len;

    attribute = (pjmedia_sdp_attr*) pj_pool_zalloc (memPool, sizeof (pjmedia_sdp_attr));

    attribute->name = pj_strdup3 (memPool, "zrtp-hash");

    /* Format: ":version value" */
    len = pj_ansi_snprintf (tempbuf, sizeof (tempbuf),
                            "%.*s %.*s",
                            4,
                            ZRTP_VERSION,
                            (int) hash.size(),
                            hash.c_str());

    attribute->value.slen = len;
    attribute->value.ptr = (char*) pj_pool_alloc (memPool, attribute->value.slen+1);
    pj_memcpy (attribute->value.ptr, tempbuf, attribute->value.slen+1);

    if (pjmedia_sdp_media_add_attr (media, attribute) != PJ_SUCCESS) {
        throw SdpException ("Could not add zrtp attribute to media");
    }
}

std::string Sdp::mediaToString (void)
{
    int size, i;
    std::ostringstream res;

    size = localAudioMediaCap.size();

    for (i = 0; i < size ; i++) {
        res << localAudioMediaCap[i]->to_string();
    }

    res << std::endl;

    return res.str();
}

void Sdp::clean_session_media()
{
    _info ("SDP: Clean session media");

    if (sessionAudioMedia.size() > 0) {

        std::vector<sdpMedia *>::iterator iter = sessionAudioMedia.begin();
        sdpMedia *media;

        while (iter != sessionAudioMedia.end()) {
            _debug ("delete media");
            media = *iter;
            delete media;
            iter++;
        }

        sessionAudioMedia.clear();
    }
}


void Sdp::cleanLocalMediaCapabilities()
{
    _info ("SDP: Clean local media capabilities");

    if (localAudioMediaCap.size() > 0) {

        std::vector<sdpMedia *>::iterator iter = localAudioMediaCap.begin();
        sdpMedia *media;

        while (iter != localAudioMediaCap.end()) {
            media = *iter;
            delete media;
            iter++;
        }

        localAudioMediaCap.clear();
    }
}

void Sdp::toString (void)
{

    std::ostringstream sdp;
    int count, i;

    sdp <<  "origin= " <<  localSession->origin.user.ptr << "\n";
    sdp << "origin.id= " << localSession->origin.id << "\n";
    sdp << "origin.version= " << localSession->origin.version<< "\n";
    sdp << "origin.net_type= " << localSession->origin.net_type.ptr<< "\n";
    sdp << "origin.addr_type= " << localSession->origin.addr_type.ptr<< "\n";

    sdp << "name=" << localSession->name.ptr<< "\n";

    sdp << "conn.net_type=" << localSession->conn->net_type.ptr<< "\n";
    sdp << "conn.addr_type=" << localSession->conn->addr_type.ptr<< "\n";
    sdp << "conn.addr=" << localSession->conn->addr.ptr<< "\n";

    sdp << "start=" <<localSession->time.start<< "\n";
    sdp << "stop=" <<localSession->time.stop<< "\n";

    sdp << "attr_count=" << localSession->attr_count << "\n";
    sdp << "media_count=" << localSession->media_count << "\n";
    sdp << "m=" << localSession->media[0]->desc.media.ptr << " ";
    sdp << localSession->media[0]->desc.port << " ";
    sdp << localSession->media[0]->desc.transport.ptr << " ";
    count = localSession->media[0]->desc.fmt_count;

    for (i=0; i<count; i++) {
        sdp << localSession->media[0]->desc.fmt[i].ptr << " ";
    }

    sdp << "\n";

    _debug ("LOCAL SDP: \n%s", sdp.str().c_str());
}

void Sdp::setPortToAllMedia (int port)
{

    std::vector<sdpMedia*> medias;
    int i, size;

    setLocalPublishedAudioPort (port);

    medias = getLocalMediaCap ();
    size = medias.size();

    for (i=0; i<size; i++) {
        medias[i]->set_port (port);
    }
}

void Sdp::addAttributeToLocalAudioMedia(std::string attr)
{
    pjmedia_sdp_attr *attribute;

    attribute = pjmedia_sdp_attr_create (memPool, attr.c_str(), NULL);

	pjmedia_sdp_media_add_attr (getLocalSdpSession()->media[0], attribute);
}

void Sdp::removeAttributeFromLocalAudioMedia(std::string attr)
{
	pjmedia_sdp_media_remove_all_attr (getLocalSdpSession()->media[0], attr.c_str());

}

std::string Sdp::convertIntToString (int value)
{
    std::ostringstream result;
    result << value;
    return result.str();
}

void Sdp::setRemoteIpFromSdp (const pjmedia_sdp_session *r_sdp)
{

    std::string remote_ip (r_sdp->conn->addr.ptr, r_sdp->conn->addr.slen);
    _info ("SDP: Remote IP from fetching SDP: %s",  remote_ip.c_str());
    this->setRemoteIP (remote_ip);
}

void Sdp::setRemoteAudioPortFromSdp (pjmedia_sdp_media *r_media)
{

    int remote_port;

    remote_port = r_media->desc.port;
    _info ("SDP: Remote Audio Port from fetching SDP: %d", remote_port);
    this->setRemoteAudioPort (remote_port);
}

void Sdp::setMediaTransportInfoFromRemoteSdp (const pjmedia_sdp_session *remote_sdp)
{

    _info ("SDP: Fetching media from sdp");

    if (!remote_sdp)
        return;

    pjmedia_sdp_media *r_media;

    getRemoteSdpMediaFromOffer (remote_sdp, &r_media);

    if (r_media==NULL) {
        _warn ("SDP: Error: no remote sdp media found in the remote offer");
        return;
    }

    setRemoteAudioPortFromSdp (r_media);

    setRemoteIpFromSdp (remote_sdp);

}

void Sdp::getRemoteSdpMediaFromOffer (const pjmedia_sdp_session* remote_sdp, pjmedia_sdp_media** r_media)
{
    int count, i;

    if (!remote_sdp)
        return;

    count = remote_sdp->media_count;
    *r_media =  NULL;

    for (i = 0; i < count; ++i) {
        if (pj_stricmp2 (&remote_sdp->media[i]->desc.media, "audio") == 0) {
            *r_media = remote_sdp->media[i];
            return;
        }
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

