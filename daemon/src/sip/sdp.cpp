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
#include "logger.h"
#include "manager.h"
#include <cassert>

Sdp::Sdp(pj_pool_t *pool)
    : memPool_(pool)
    , negotiator_(NULL)
    , localSession_(NULL)
    , remoteSession_(NULL)
    , activeLocalSession_(NULL)
    , activeRemoteSession_(NULL)
    , codec_list_()
    , sessionAudioMedia_()
    , localIpAddr_()
    , remoteIpAddr_()
    , localAudioPort_(0)
    , remoteAudioPort_(0)
    , zrtpHelloHash_()
    , srtpCrypto_()
    , telephoneEventPayload_(101) // same as asterisk
{}

void Sdp::setActiveLocalSdpSession(const pjmedia_sdp_session *sdp)
{
    activeLocalSession_ = (pjmedia_sdp_session*) sdp;

    if (activeLocalSession_->media_count < 1)
        return;

    pjmedia_sdp_media *current = activeLocalSession_->media[0];

    for (unsigned j = 0; j < current->desc.fmt_count; j++) {
        static const pj_str_t STR_RTPMAP = { (char*) "rtpmap", 6 };
        pjmedia_sdp_attr *attribute = pjmedia_sdp_media_find_attr(current, &STR_RTPMAP, NULL);

        if (!attribute) {
            sessionAudioMedia_.clear();
            return;
        }

        pjmedia_sdp_rtpmap *rtpmap;
        pjmedia_sdp_attr_to_rtpmap(memPool_, attribute, &rtpmap);

        sfl::Codec *codec = Manager::instance().audioCodecFactory.getCodec((int) pj_strtoul(&rtpmap->pt));

        if (!codec) {
            sessionAudioMedia_.clear();
            return;
        }

        sessionAudioMedia_.push_back(codec);
    }
}



void Sdp::setActiveRemoteSdpSession(const pjmedia_sdp_session *sdp)
{
    activeRemoteSession_ = (pjmedia_sdp_session*) sdp;

    if (!sdp) {
        ERROR("Remote sdp is NULL while parsing telephone event attribute");
        return;
    }

    for (unsigned i = 0; i < sdp->media_count; i++)
        if (pj_stricmp2(&sdp->media[i]->desc.media, "audio") == 0) {
            pjmedia_sdp_media *r_media = sdp->media[i];
            static const pj_str_t STR_TELEPHONE_EVENT = { (char*) "telephone-event", 15};
            pjmedia_sdp_attr *attribute = pjmedia_sdp_attr_find(r_media->attr_count, r_media->attr, &STR_TELEPHONE_EVENT, NULL);

            if (attribute != NULL) {
                pjmedia_sdp_rtpmap *rtpmap;
                pjmedia_sdp_attr_to_rtpmap(memPool_, attribute, &rtpmap);
                telephoneEventPayload_ = pj_strtoul(&rtpmap->pt);
            }

            return;
        }

    ERROR("Could not found dtmf event from remote sdp");
}

std::string Sdp::getCodecName()
{
    try {
        sfl::AudioCodec *codec = getSessionMedia();
        return codec ? codec->getMimeSubtype() : "";
    } catch (...) {
        return "";
    }
}

sfl::AudioCodec* Sdp::getSessionMedia()
{
    if (sessionAudioMedia_.size() < 1)
        throw SdpException("No codec description for this media");

    return dynamic_cast<sfl::AudioCodec *>(sessionAudioMedia_[0]);
}


pjmedia_sdp_media *Sdp::setMediaDescriptorLine()
{
    pjmedia_sdp_media *med = PJ_POOL_ZALLOC_T(memPool_, pjmedia_sdp_media);

    med->desc.media = pj_str((char*)"audio");
    med->desc.port_count = 1;
    med->desc.port = localAudioPort_;
    // in case of sdes, media are tagged as "RTP/SAVP", RTP/AVP elsewhere
    med->desc.transport = pj_str(srtpCrypto_.empty() ? (char*)"RTP/AVP" : (char*)"RTP/SAVP");

    med->desc.fmt_count = codec_list_.size();

    for (unsigned i=0; i<med->desc.fmt_count; i++) {
        sfl::Codec *codec = codec_list_[i];

        std::ostringstream result;
        result << (int)codec->getPayloadType();

        pj_strdup2(memPool_, &med->desc.fmt[i], result.str().c_str());

        // Add a rtpmap field for each codec
        // We could add one only for dynamic payloads because the codecs with static RTP payloads
        // are entirely defined in the RFC 3351, but if we want to add other attributes like an asymmetric
        // connection, the rtpmap attribute will be useful to specify for which codec it is applicable
        pjmedia_sdp_rtpmap rtpmap;
        rtpmap.pt = med->desc.fmt[i];
        rtpmap.enc_name = pj_str((char*) codec->getMimeSubtype().c_str());

        // G722 require G722/8000 media description even if it is 16000 codec
        if (codec->getPayloadType() == 9) {
            rtpmap.clock_rate = 8000;
        } else {
            rtpmap.clock_rate = codec->getClockRate();
        }

        rtpmap.param.ptr = ((char* const)"");
        rtpmap.param.slen = 0;

        pjmedia_sdp_attr *attr;
        pjmedia_sdp_rtpmap_to_attr(memPool_, &rtpmap, &attr);

        med->attr[med->attr_count++] = attr;
    }

    med->attr[ med->attr_count++] = pjmedia_sdp_attr_create(memPool_, "sendrecv", NULL);

    if (!zrtpHelloHash_.empty())
        addZrtpAttribute(med, zrtpHelloHash_);

    setTelephoneEventRtpmap(med);

    return med;
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

void Sdp::setLocalMediaCapabilities(const CodecOrder &selectedCodecs)
{
    if (selectedCodecs.empty())
        WARN("No selected codec while building local SDP offer");

    codec_list_.clear();

    for (CodecOrder::const_iterator iter = selectedCodecs.begin(); iter != selectedCodecs.end(); ++iter) {
        sfl::Codec *codec = Manager::instance().audioCodecFactory.getCodec(*iter);

        if (codec)
            codec_list_.push_back(codec);
        else
            WARN("SDP: Couldn't find audio codec");
    }
}

namespace {
    void printSession(const pjmedia_sdp_session *session)
    {
        char buffer[2048];
        size_t size = pjmedia_sdp_print(session, buffer, sizeof(buffer));
        std::string sessionStr(buffer, std::min(size, sizeof(buffer)));
        DEBUG("%s", sessionStr.c_str());
    }
}

int Sdp::createLocalSession(const CodecOrder &selectedCodecs)
{
    setLocalMediaCapabilities(selectedCodecs);

    localSession_ = PJ_POOL_ZALLOC_T(memPool_, pjmedia_sdp_session);
    if (!localSession_) {
        ERROR("Could not create local SDP session");
        return !PJ_SUCCESS;
    }

    localSession_->conn = PJ_POOL_ZALLOC_T(memPool_, pjmedia_sdp_conn);

    /* Initialize the fields of the struct */
    localSession_->origin.version = 0;
    pj_time_val tv;
    pj_gettimeofday(&tv);

    localSession_->origin.user = pj_str(pj_gethostname()->ptr);
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
    localSession_->media_count = 1;
    localSession_->media[0] = setMediaDescriptorLine();

    if (!srtpCrypto_.empty())
        addSdesAttribute(srtpCrypto_);

    DEBUG("Local SDP Session:");
    printSession(localSession_);

    return pjmedia_sdp_validate(localSession_);
}

void Sdp::createOffer(const CodecOrder &selectedCodecs)
{
    if (createLocalSession(selectedCodecs) != PJ_SUCCESS)
        ERROR("Failed to create initial offer");
    else if (pjmedia_sdp_neg_create_w_local_offer(memPool_, localSession_, &negotiator_) != PJ_SUCCESS)
        ERROR("Failed to create an initial SDP negotiator");
}

void Sdp::receiveOffer(const pjmedia_sdp_session* remote,
                       const CodecOrder &selectedCodecs)
{
    if (!remote) {
        ERROR("Remote session is NULL");
        return;
    }

    DEBUG("Remote SDP Session:");
    printSession(remote);

    if (localSession_ == NULL && createLocalSession(selectedCodecs) != PJ_SUCCESS) {
        ERROR("Failed to create initial offer");
        return;
    }

    remoteSession_ = pjmedia_sdp_session_clone(memPool_, remote);

    if (pjmedia_sdp_neg_create_w_remote_offer(memPool_, localSession_,
                remoteSession_, &negotiator_) != PJ_SUCCESS) {
        ERROR("Could not create negotiator with remote offer");
        negotiator_ = NULL;
    }
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

void Sdp::addSdesAttribute(const std::vector<std::string>& crypto)
{
    for (std::vector<std::string>::const_iterator iter = crypto.begin();
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

Sdp::~Sdp()
{
}

void Sdp::addAttributeToLocalAudioMedia(const char *attr)
{
    if (localSession_)
        pjmedia_sdp_media_add_attr(localSession_->media[0], pjmedia_sdp_attr_create(memPool_, attr, NULL));
}

void Sdp::removeAttributeFromLocalAudioMedia(const char *attr)
{
    if (localSession_)
        pjmedia_sdp_media_remove_all_attr(localSession_->media[0], attr);
}

void Sdp::setMediaTransportInfoFromRemoteSdp()
{
    if (!activeRemoteSession_) {
        ERROR("Remote sdp is NULL while parsing media");
        return;
    }

    for (unsigned i = 0; i < activeRemoteSession_->media_count; ++i)
        if (pj_stricmp2(&activeRemoteSession_->media[i]->desc.media, "audio") == 0) {
            setRemoteAudioPort(activeRemoteSession_->media[i]->desc.port);
            setRemoteIP(std::string(activeRemoteSession_->conn->addr.ptr, activeRemoteSession_->conn->addr.slen));
            return;
        }

    ERROR("No remote sdp media found in the remote offer");
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
