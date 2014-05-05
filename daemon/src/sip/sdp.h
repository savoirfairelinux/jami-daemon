/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef SDP_H_
#define SDP_H_

#include "global.h"
#include "noncopyable.h"
#include "ip_utils.h"

#include <pjmedia/sdp.h>
#include <pjmedia/sdp_neg.h>
#include <pjsip/sip_transport.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjmedia/errno.h>
#include <pj/pool.h>
#include <pj/assert.h>

#include <vector>
#include <string>
#include <stdexcept>

namespace sfl {
class AudioCodec;
}

class SdpException : public std::runtime_error {
    public:
        SdpException(const std::string& str="") :
            std::runtime_error("SDP: SdpException occured: " + str) {}
};

typedef std::vector<std::string> CryptoOffer;

class Sdp {
    public:

        /*
         * Class Constructor.
         *
         * @param memory pool
         */
        Sdp(pj_pool_t *pool);

        ~Sdp();

        /**
         * Accessor for the internal memory pool
         */
        pj_pool_t *getMemoryPool() const {
            return memPool_;
        }

        /**
         *  Read accessor. Get the local passive sdp session information before negotiation
         *
         *  @return The structure that describes a SDP session
         */
        pjmedia_sdp_session *getLocalSdpSession() {
            return localSession_;
        }

        /**
         * Read accessor. Get the remote passive sdp session information before negotiation
         *
         * @return The structure that describe the SDP session
         */
        pjmedia_sdp_session *getRemoteSdpSession() {
            return remoteSession_;
        }

        /**
         * Set the negotiated sdp offer from the sip payload.
         *
         * @param sdp   the negotiated offer
         */
        void setActiveLocalSdpSession(const pjmedia_sdp_session *sdp);

        /**
         * Retrieve the negotiated sdp offer from the sip payload.
         *
         * @param sdp   the negotiated offer
         */
        void setActiveRemoteSdpSession(const pjmedia_sdp_session *sdp);

        /**
         * Returns a string version of the negotiated SDP fields which pertain
         * to video.
         */
        std::string getIncomingVideoDescription() const;

        /*
         * On building an invite outside a dialog, build the local offer and create the
         * SDP negotiator instance with it.
         * @returns true if offer was created, false otherwise
         */
        bool
        createOffer(const std::vector<int> &selectedCodecs,
                    const std::vector<std::map<std::string, std::string> > &videoCodecs);

        /*
        * On receiving an invite outside a dialog, build the local offer and create the
        * SDP negotiator instance with the remote offer.
        *
        * @param remote    The remote offer
        */
        void receiveOffer(const pjmedia_sdp_session* remote,
                          const std::vector<int> &selectedCodecs,
                          const std::vector<std::map<std::string, std::string> > &videoCodecs);

        /**
         * Start the sdp negotiation.
         */
        void startNegotiation();

        /**
         * Remove all media in the session media vector.
         */
        void cleanSessionMedia();

        /**
         * Remove all media in local media capability vector
         */
        void cleanLocalMediaCapabilities();

        /*
         * Write accessor. Set the local IP address that will be used in the sdp session
         */
        void setPublishedIP(const std::string &addr, pj_uint16_t addr_type =  pj_AF_UNSPEC());
        void setPublishedIP(const IpAddr& addr);

        /*
         * Read accessor. Get the local IP address
         */
        IpAddr getPublishedIPAddr() const {
            return publishedIpAddr_;
        }

        std::string getPublishedIP() const {
            return publishedIpAddr_;
        }

        void setLocalPublishedAudioPort(int port) {
            localAudioDataPort_ = port;
            localAudioControlPort_ = port + 1;
        }

        void setLocalPublishedVideoPort (int port) {
            localVideoDataPort_ = port;
            localVideoControlPort_ = port + 1;
        }

        void updatePorts(const std::vector<pj_sockaddr> &sockets);

        /**
         * Return IP of destination
         * @return const std:string	The remote IP address
         */
        const std::string& getRemoteIP() {
            return remoteIpAddr_;
        }

        /**
         * Set remote's audio port. [not protected]
         * @param port  The remote audio port
         */
        void setRemoteAudioPort(unsigned int port) {
            remoteAudioPort_ = port;
        }

        /**
         * Return audio port at destination [mutex protected]
         * @return unsigned int The remote audio port
         */
        unsigned int getRemoteAudioPort() const {
            return remoteAudioPort_;
        }

        /**
         * Return video port at destination
         * @return unsigned int The remote video port
         */
        unsigned int getRemoteVideoPort() const {
            return remoteVideoPort_;
        }

        unsigned int getLocalVideoPort() const {
            return localVideoDataPort_;
        }

        void addAttributeToLocalAudioMedia(const char *attr);
        void removeAttributeFromLocalAudioMedia(const char *attr);
        void addAttributeToLocalVideoMedia(const char *attr);
        void removeAttributeFromLocalVideoMedia(const char *attr);

        /**
         * Get SRTP master key
         * @param remote sdp session
         * @param crypto offer
         */
        void getRemoteSdpCryptoFromOffer(const pjmedia_sdp_session* remote_sdp, CryptoOffer& crypto_offer);

        /**
         * Set the SRTP master_key
         * @param mk The Master Key of a srtp session.
         */
        void setLocalSdpCrypto(const std::vector<std::string> &lc) {
            srtpCrypto_ = lc;
        }

        /**
         * Set the zrtp hash that was previously calculated from the hello message in the zrtp layer.
         * This hash value is unique at the media level. Therefore, if video support is added, one would
         * have to set the correct zrtp-hash value in the corresponding media section.
         * @param hash The hello hash of a rtp session. (Only audio at the moment)
         */
        void setZrtpHash(const std::string& hash) {
            zrtpHelloHash_ = hash;
        }

        unsigned int getTelephoneEventType() const {
            return telephoneEventPayload_;
        }

        void setMediaTransportInfoFromRemoteSdp();

        std::string getSessionVideoCodec() const;
        std::vector<sfl::AudioCodec*> getSessionAudioMedia() const;
        // Sets @param settings with appropriate values and returns true if
        // we are sending video, false otherwise
        bool getOutgoingVideoSettings(std::map<std::string, std::string> &settings) const;

    private:
        NON_COPYABLE(Sdp);
        friend class SDPTest;

        std::string getLineFromSession(const pjmedia_sdp_session *sess, const std::string &keyword) const;
        std::string getOutgoingVideoCodec() const;
        std::string getOutgoingVideoField(const std::string &codec, const char *key) const;
        int getOutgoingVideoPayload() const;
        void getProfileLevelID(const pjmedia_sdp_session *session, std::string &dest, int payload) const;
        void updateRemoteIP(unsigned index);

        /**
         * The pool to allocate memory, ownership to SipCall
         * SDP should not release the pool itself
         */
        pj_pool_t *memPool_;

        /** negotiator */
        pjmedia_sdp_neg *negotiator_;

        /**
         * Local SDP
         */
        pjmedia_sdp_session *localSession_;

        /**
         * Remote SDP
         */
        pjmedia_sdp_session *remoteSession_;

        /**
         * The negotiated SDP remote session
         * Explanation: each endpoint's offer is negotiated, and a new sdp offer results from this
         * negotiation, with the compatible media from each part
         */
        const pjmedia_sdp_session *activeLocalSession_;

        /**
         * The negotiated SDP remote session
         * Explanation: each endpoint's offer is negotiated, and a new sdp offer results from this
         * negotiation, with the compatible media from each part
         */
        const pjmedia_sdp_session *activeRemoteSession_;

        /**
         * Codec Map used for offer
         */
        std::vector<sfl::AudioCodec *> audio_codec_list_;
        std::vector<std::map<std::string, std::string> > video_codec_list_;

        /**
         * The codecs that will be used by the session (after the SDP negotiation)
         */
        std::vector<sfl::AudioCodec *> sessionAudioMediaLocal_;
        std::vector<sfl::AudioCodec *> sessionAudioMediaRemote_;
        std::vector<std::string> sessionVideoMedia_;

        std::string publishedIpAddr_;
        pj_uint16_t publishedIpAddrType_;

        std::string remoteIpAddr_;

        int localAudioDataPort_;
        int localAudioControlPort_;
        int localVideoDataPort_;
        int localVideoControlPort_;
        unsigned int remoteAudioPort_;
        unsigned int remoteVideoPort_;

        std::string zrtpHelloHash_;

        /**
         * "a=crypto" sdes local attributes obtained from AudioSrtpSession
         */
        std::vector<std::string> srtpCrypto_;

        unsigned int telephoneEventPayload_;

        /*
         * Build the sdp media section
         * Add rtpmap field if necessary
         */
        pjmedia_sdp_media *setMediaDescriptorLines(bool audio);

        void setTelephoneEventRtpmap(pjmedia_sdp_media *med);

        /**
         * Build the local media capabilities for this session
         * @param List of codec in preference order
         */
        void setLocalMediaAudioCapabilities(const std::vector<int> &selected);
        void setLocalMediaVideoCapabilities(const std::vector<std::map<std::string, std::string> > &codecs);
        /*
         * Build the local SDP offer
         */
        int createLocalSession(const std::vector<int> &selectedAudio,
                               const std::vector<std::map<std::string, std::string> > &selectedVideo);
        /*
         * Adds a sdes attribute to the given media section.
         *
         * @param media The media to add the srtp attribute to
         * @throw SdpException
         */
        void addSdesAttribute(const std::vector<std::string>& crypto);

        /*
         * Adds a zrtp-hash  attribute to
         * the given media section. The hello hash is
         * available only after is has been computed
         * in the AudioZrtpSession constructor.
         *
         * @param media The media to add the zrtp-hash attribute to
         * @param hash  The hash to which the attribute should be set to
         * @throw SdpException
         */
        void addZrtpAttribute(pjmedia_sdp_media* media, std::string hash);

        void addRTCPAttribute(pjmedia_sdp_media *med);
};


#endif
