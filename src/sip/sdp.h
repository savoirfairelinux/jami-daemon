/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
 */

#pragma once

#include "noncopyable.h"
#include "sdes_negotiator.h"
#include "sip_utils.h"
#include "ip_utils.h"
#include "ice_transport.h"
#include "media_codec.h"
#include "sip_utils.h"

#include <pjmedia/sdp.h>
#include <pjmedia/sdp_neg.h>
#include <pjsip/sip_transport.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjmedia/errno.h>
#include <pj/pool.h>
#include <pj/assert.h>

#include <map>
#include <vector>
#include <string>
#include <stdexcept>

namespace ring {

namespace test {
class SDPTest;
}

class AudioCodec;

class SdpException : public std::runtime_error {
    public:
        SdpException(const std::string& str="") :
            std::runtime_error("SDP: SdpException occurred: " + str) {}
};

class Sdp {
    public:

        /*
         * Class Constructor.
         *
         * @param memory pool
         */
        Sdp(const std::string& id);

        ~Sdp();

        /**
         *  Read accessor. Get the local passive sdp session information before negotiation
         *
         *  @return The structure that describes a SDP session
         */
        pjmedia_sdp_session *getLocalSdpSession() {
            return localSession_;
        }

        const pjmedia_sdp_session *getActiveLocalSdpSession() const {
            return activeLocalSession_;
        }

        /**
         * Read accessor. Get the remote passive sdp session information before negotiation
         *
         * @return The structure that describe the SDP session
         */
        pjmedia_sdp_session *getRemoteSdpSession() {
            return remoteSession_;
        }

        const pjmedia_sdp_session *getActiveRemoteSdpSession() const {
            return activeRemoteSession_;
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

        /*
         * On building an invite outside a dialog, build the local offer and create the
         * SDP negotiator instance with it.
         * @returns true if offer was created, false otherwise
         */
        bool createOffer(const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedAudioCodecs,
                         const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedVideoCodecs,
                         sip_utils::KeyExchangeProtocol,
                         bool holding=false);

        /*
        * On receiving an invite outside a dialog, build the local offer and create the
        * SDP negotiator instance with the remote offer.
        *
        * @param remote    The remote offer
        */
        void receiveOffer(const pjmedia_sdp_session* remote,
                          const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedAudioCodecs,
                          const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedVideoCodecs,
                          sip_utils::KeyExchangeProtocol,
                          bool holding=false);

        /**
         * Start the sdp negotiation.
         */
        void startNegotiation();

        /**
         * Remove all media in the session media vector.
         */
        void cleanSessionMedia();

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

        void setLocalPublishedAudioPorts(int audio_port, int control_port) {
            localAudioDataPort_ = audio_port;
            localAudioControlPort_ = control_port;
        }

        void setLocalPublishedVideoPort (int port) {
            localVideoDataPort_ = port;
            localVideoControlPort_ = port + 1;
        }

        void setLocalPublishedVideoPorts(int video_port, int control_port) {
            localVideoDataPort_ = video_port;
            localVideoControlPort_ = control_port;
        }

        unsigned int getLocalVideoPort() const {
            return localVideoDataPort_;
        }

        unsigned int getLocalVideoControlPort() const {
            return localVideoControlPort_;
        }

        unsigned int getLocalAudioPort() const {
            return localAudioDataPort_;
        }

        unsigned int getLocalAudioControlPort() const {
            return localAudioControlPort_;
        }

        std::vector<MediaDescription>
        getMediaSlots(const pjmedia_sdp_session* session, bool remote) const;

        using MediaSlot = std::pair<MediaDescription, MediaDescription>;
        std::vector<MediaSlot> getMediaSlots() const;

        unsigned int getTelephoneEventType() const {
            return telephoneEventPayload_;
        }

        void addIceAttributes(const IceTransport::Attribute&& ice_attrs);
        IceTransport::Attribute getIceAttributes() const;
        static IceTransport::Attribute getIceAttributes(const pjmedia_sdp_session* session);

        void addIceCandidates(unsigned media_index,
                              const std::vector<std::string>& cands);

        std::vector<std::string> getIceCandidates(unsigned media_index) const;

        void clearIce();

        /// \brief Log the given session
        /// \note crypto lines with are removed for security
        static void printSession(const pjmedia_sdp_session *session, const char* header);

    private:
        friend class test::SDPTest;

        NON_COPYABLE(Sdp);

        std::string getLineFromSession(const pjmedia_sdp_session *sess, const std::string &keyword) const;
        std::string getOutgoingVideoField(const std::string &codec, const char *key) const;
        void getProfileLevelID(const pjmedia_sdp_session *session, std::string &dest, int payload) const;

        /**
         * Returns the printed original SDP filtered with only the specified media index and codec remaining.
         */
        static std::string getFilteredSdp(const pjmedia_sdp_session* session, unsigned media_keep, unsigned pt_keep);

        static void clearIce(pjmedia_sdp_session* session);

        /**
         * The pool to allocate memory
         */
        std::unique_ptr<pj_pool_t, std::function<void(pj_pool_t*)>> memPool_;

        /** negotiator */
        pjmedia_sdp_neg *negotiator_ {nullptr};

        /**
         * Local SDP
         */
        pjmedia_sdp_session *localSession_ {nullptr};

        /**
         * Remote SDP
         */
        pjmedia_sdp_session *remoteSession_ {nullptr};

        /**
         * The negotiated SDP remote session
         * Explanation: each endpoint's offer is negotiated, and a new sdp offer results from this
         * negotiation, with the compatible media from each part
         */
        const pjmedia_sdp_session *activeLocalSession_ {nullptr};

        /**
         * The negotiated SDP remote session
         * Explanation: each endpoint's offer is negotiated, and a new sdp offer results from this
         * negotiation, with the compatible media from each part
         */
        const pjmedia_sdp_session *activeRemoteSession_ {nullptr};

        /**
         * Codec Map used for offer
         */
        std::vector<std::shared_ptr<AccountCodecInfo>> audio_codec_list_;
        std::vector<std::shared_ptr<AccountCodecInfo>> video_codec_list_;

        std::string publishedIpAddr_;
        pj_uint16_t publishedIpAddrType_;

        int localAudioDataPort_ {0};
        int localAudioControlPort_ {0};
        int localVideoDataPort_ {0};
        int localVideoControlPort_ {0};

        SdesNegotiator sdesNego_;

        unsigned int telephoneEventPayload_;

        /*
         * Build the sdp media section
         * Add rtpmap field if necessary
         */
        pjmedia_sdp_media *setMediaDescriptorLines(bool audio, bool holding, sip_utils::KeyExchangeProtocol);
        pjmedia_sdp_attr *generateSdesAttribute();

        void setTelephoneEventRtpmap(pjmedia_sdp_media *med);

        /**
         * Build the local media capabilities for this session
         * @param List of codec in preference order
         */
        void setLocalMediaAudioCapabilities(const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedAudioCodecs);
        void setLocalMediaVideoCapabilities(const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedVideoCodecs);

        /*
         * Build the local SDP offer
         */
        int createLocalSession(const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedAudioCodecs,
                               const std::vector<std::shared_ptr<AccountCodecInfo>>& selectedVideoCodecs,
                               sip_utils::KeyExchangeProtocol,
                               bool holding);

        /*
         * Adds a sdes attribute to the given media section.
         *
         * @param media The media to add the srtp attribute to
         * @throw SdpException
         */
        void addSdesAttribute(const std::vector<std::string>& crypto);

        void addRTCPAttribute(pjmedia_sdp_media *med);

        std::shared_ptr<AccountCodecInfo> findCodecByPayload(const unsigned payloadType);
        std::shared_ptr<AccountCodecInfo> findCodecBySpec(const std::string &codecName, const unsigned clockrate=0) const;
};

} // namespace ring
