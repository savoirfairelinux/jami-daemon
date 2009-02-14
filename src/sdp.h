/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 */

#ifndef _SDP_H
#define _SDP_H

#include <pjmedia/sdp.h>
#include <pjmedia/sdp_neg.h>
#include <pjsip/sip_transport.h>
#include <pjlib.h>
#include <pj/pool.h>
#include <pj/assert.h>

#include "audio/codecDescriptor.h"

class Sdp {

    public:

        Sdp(pj_pool_t *pool);

        ~Sdp();

        /**
         * Setup incoming call, and verify for errors, before ringing the user.
         * @param pjsip_rx_data *rdata
         * @param pj_pool_t *pool
         * @return bool True on success
         *		    false otherwise
         */
        bool SIPCallInvite(pjsip_rx_data *rdata);

        bool SIPCallAnsweredWithoutHold(pjsip_rx_data *rdata);

        /**
         * Get the local SDP 
         * @param void
         * @return _localSDP pjmedia_sdp_session
         */
        pjmedia_sdp_session* getLocalSDPSession( void ) { return _localSDP; }

        /**
         * Begin negociation of media information between caller and callee
         * @param pj_pool_t *pool
         * @return bool True if ok
         */
        bool startNegociation();

        /**
         * Create the localSDP, media negociation and codec information
         * @param pj_pool_t *pool
         * @return void
         */
        bool createInitialOffer();

        /** 
         * Set internal codec Map: initialization only, not protected 
         * @param map The codec map
         */
        void setCodecMap(const CodecDescriptor& map) { _codecMap = map; } 

        /**
         * Save IP Address
         * @param ip std::string 
         * @return void
         */
        void setIp(std::string ip) {_ipAddr = ip;}

        /** 
         * Get internal codec Map: initialization only, not protected 
         * @return CodecDescriptor	The codec map
         */
        CodecDescriptor& getCodecMap();
        
        void  setLocalExternAudioPort(int port){ _localPort = port; }
        
        int  getLocalExternAudioPort (void){ return _localPort; }

        int receiving_initial_offer( pjmedia_sdp_session* remote );

    private:
        /** 
         * Set the audio codec used.  [not protected] 
         * @param audioCodec  The payload of the codec
         */
        void setAudioCodec(AudioCodecType audioCodec) { _audioCodec = audioCodec; }

        /** Codec pointer */
        AudioCodecType _audioCodec;

        /** IP address */
        std::string _ipAddr;

        int _localPort;

        /**
         * Get a valid remote media
         * @param remote_sdp pjmedia_sdp_session*
         * @return pjmedia_sdp_media*. A valid sdp_media_t or 0
         */
        pjmedia_sdp_media* getRemoteMedia(pjmedia_sdp_session *remote_sdp);

        pjmedia_sdp_session * getRemoteSDPFromRequest (pjsip_rx_data *rdata);

        /**
         * Set Audio Port and Audio IP from Remote SDP Info
         * @param remote_med Remote Media info
         * @param remote_sdp Remote SDP pointer
         * @return bool True if everything is set correctly
         */
        bool setRemoteAudioFromSDP(pjmedia_sdp_session* remote_sdp, pjmedia_sdp_media* remote_med);

        /**
         * Set Audio Codec with the remote choice
         * @param remote_med Remote Media info
         * @return bool True if everything is set correctly
         */
        bool setAudioCodecFromSDP(pjmedia_sdp_media* remote_med);

        /** Local SDP */
        pjmedia_sdp_session *_localSDP;
        pjmedia_sdp_session *_negociated_offer;

        /** negociator */
        pjmedia_sdp_neg *_negociator;

        // The pool to allocate memory
        pj_pool_t *_pool;

        /** Codec Map */
        CodecDescriptor _codecMap;

        /**
         * Set origin information for local SDP
         */
        void sdpAddOrigin( void );

        /**
         * Set connection information for local SDP
         */
        void sdpAddConnectionInfo( void );
        /**
         * Set media information including codec for localSDP
         * @param  pj_pool_t* pool
         * @return void
         */
        void sdpAddMediaDescription();

};


#endif
