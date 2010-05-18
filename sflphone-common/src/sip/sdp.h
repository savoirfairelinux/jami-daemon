/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#ifndef _SDP_H
#define _SDP_H

#include <pjmedia/sdp.h>
#include <pjmedia/sdp_neg.h>
#include <pjsip/sip_transport.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjmedia/errno.h>
#include <pj/pool.h>
#include <pj/assert.h>
#include <vector>

#include "audio/codecs/codecDescriptor.h"
#include "sdpmedia.h"

#include <exception>

class sdpException: public std::exception
{
  virtual const char* what() const throw()
  {
    return "An sdpException Occured";
  }
};

typedef std::vector<std::string> CryptoOffer;

class Sdp {

    public:
        
        /*
         * Class Constructor.
         *
         * @param ip_addr
         */
        Sdp(pj_pool_t *pool);

        /* Class destructor */
        ~Sdp();

        /*
         * Read accessor. Get the list of the local media capabilities. 
         *
         * @return std::vector<sdpMedia*>   the vector containing the different media
         */
        std::vector<sdpMedia*> get_local_media_cap( void ) { return _local_media_cap; }

         /*
         *  Read accessor. Get the sdp session information
         *
         *  @return pjmedia_sdp_session   The structure that describes a SDP session
         */
        pjmedia_sdp_session* get_local_sdp_session( void ) { return _local_offer; }

        /*
         * Write accessor. Set the local IP address that will be used in the sdp session
         */
        void set_ip_address( std::string ip_addr ) { _ip_addr = ip_addr; }

        /*
         * Read accessor. Get the local IP address
         */
        std::string get_ip_address( void ) { return _ip_addr; }
        
        /*
         * Build the local SDP offer
         */
        int create_local_offer (CodecOrder selectedCodecs);

        /*
         * Build the sdp media section
         * Add rtpmap field if necessary
         *
         * @param media The media to add to SDP
         * @param med   The structure to receive the media section
         */
        void set_media_descriptor_line( sdpMedia* media, pjmedia_sdp_media** p_med );

        /* Set the zrtp hash that was previously calculated from the hello message in the zrtp layer.
         * This hash value is unique at the media level. Therefore, if video support is added, one would
         * have to set the correct zrtp-hash value in the corresponding media section.
         * @param hash The hello hash of a rtp session. (Only audio at the moment)
         */
        inline void set_zrtp_hash(const std::string& hash) { _zrtp_hello_hash = hash; _debug("Zrtp hash set with %s\n", hash.c_str()); }

	/* Set the srtp _master_key
         * @param mk The Master Key of a srtp session.
         */
        inline void set_srtp_crypto(const std::vector<std::string> lc) { _srtp_crypto = lc; }
        
        /*
         * On building an invite outside a dialog, build the local offer and create the
         * SDP negociator instance with it.
         */
        int create_initial_offer (CodecOrder selectedCodecs);

         /*
         * On receiving an invite outside a dialog, build the local offer and create the
         * SDP negociator instance with the remote offer.
         *
         * @param remote    The remote offer
         */
        int receiving_initial_offer (pjmedia_sdp_session* remote, CodecOrder selectedCodecs);
        
        /*
         * On receiving a message, check if it contains SDP and negotiate. Should be used for
         * SDP answer and offer but currently is only used for answer.
         * SDP negociator instance with the remote offer.
         *
         * @param inv       The  the invitation
         * @param rdata     The remote data
         */
        
        pj_status_t check_sdp_answer(pjsip_inv_session *inv, pjsip_rx_data *rdata);
        
        /**
         * Remove all media in the session media vector.
         */
        void clean_session_media(void);

        /**
         * Remove all media in local media capability vector
         */
		void clean_local_media_capabilities(void);

        /*
         * Return a string description of the media added to the session,
         * ie the local media capabilities
         */
        std::string media_to_string( void );

        /*
         * Return the codec of the first media after negociation
         */
        AudioCodec* get_session_media( void );

        /*
         * read accessor. Return the negociated offer
         *
         * @return pjmedia_sdp_session  The negociated offer
         */
        pjmedia_sdp_session* get_negociated_offer( void ){
            return _negociated_offer;
        }

         /*
         * Start the sdp negociation.
         *
         * @return pj_status_t  0 on success
         *                      1 otherwise
         */
        pj_status_t start_negociation( void );

         /*
         * Retrieve the negociated sdp offer from the sip payload.
         *
         * @param sdp   the negociated offer
         */
        void set_negotiated_sdp ( const pjmedia_sdp_session *sdp );

        /*
         * Attribute the specified port to every medias provided
         * This is valid only because we are using one media
         * We should change this to support multiple medias
         *
         * @param port  The media port
         */
        void attribute_port_to_all_media (int port);

        void  set_local_extern_audio_port(int port){ _local_extern_audio_port = port; }

        int  get_local_extern_audio_port (void){ return _local_extern_audio_port; }

        void toString (void);

        /** 
         * Set remote's IP addr. [not protected]
         * @param ip  The remote IP address
         */
        void set_remote_ip(const std::string& ip) { _remote_ip_addr = ip; }
        
        /** 
         * Return IP of destination [mutex protected]
         * @return const std:string	The remote IP address
         */
        const std::string& get_remote_ip() { return _remote_ip_addr; }

        /** 
         * Set remote's audio port. [not protected]
         * @param port  The remote audio port
         */
        void set_remote_audio_port(unsigned int port) { _remote_audio_port = port; }

        /** 
         * Return audio port at destination [mutex protected] 
         * @return unsigned int The remote audio port
         */
        unsigned int get_remote_audio_port() { return _remote_audio_port; }

        void set_media_transport_info_from_remote_sdp (const pjmedia_sdp_session *remote_sdp);

        std::vector<sdpMedia*> get_session_media_list (void) { return _session_media; }

        void get_remote_sdp_crypto_from_offer (const pjmedia_sdp_session* remote_sdp, CryptoOffer& crypto_offer);

    private:
        /** Codec Map */
        std::vector<sdpMedia*> _local_media_cap;

        /* The media that will be used by the session (after the SDP negociation) */
        std::vector<sdpMedia*> _session_media;

        /** negociator */
        pjmedia_sdp_neg *_negociator;

        /** IP address */
        std::string _ip_addr;

        /** Remote's IP address */
        std::string  _remote_ip_addr;
        
        /** Local SDP */
        pjmedia_sdp_session *_local_offer;

        /* The negociated SDP offer */
        // Explanation: each endpoint's offer is negociated, and a new sdp offer results from this
        // negociation, with the compatible media from each part 
        pjmedia_sdp_session *_negociated_offer;

        // The pool to allocate memory
        pj_pool_t *_pool;

        /** Local audio port */
        int _local_extern_audio_port;

        /** Remote audio port */
        unsigned int _remote_audio_port;

        std::string _zrtp_hello_hash;

        /** "a=crypto" sdes local attributes obtained from AudioSrtpSession */
        std::vector<std::string> _srtp_crypto;
        
        Sdp(const Sdp&); //No Copy Constructor
        Sdp& operator=(const Sdp&); //No Assignment Operator

        void set_local_media_capabilities (CodecOrder selectedCodecs);

        /*
         *  Mandatory field: Origin ("o=")
         *  Gives the originator of the session.
         *  Serves as a globally unique identifier for this version of this session description.
         */
        void sdp_add_origin( void );

        /*
         *  Mandatory field: Protocol version ("v=")
         *  Add the protocol version in the SDP session description
         */
        void sdp_add_protocol( void );

        /*
         *  Optional field: Connection data ("c=")
         *  Contains connection data.
         */
        void sdp_add_connection_info( void );
        
        /*
         *  Mandatory field: Session name ("s=")
         *  Add a textual session name.
         */
        void sdp_add_session_name( void );

        /*
         *  Optional field: Session information ("s=")
         *  Provides textual information about the session.
         */
        void sdp_add_session_info( void ){}

        /*
         *  Optional field: Uri ("u=")
         *  Add a pointer to additional information about the session.
         */
        void sdp_add_uri( void ) {}

        /*
         *  Optional fields: Email address and phone number ("e=" and "p=")
         *  Add contact information for the person responsible for the conference.
         */
        void sdp_add_email( void ) {}

        /*
         *  Optional field: Bandwidth ("b=")
         *  Denotes the proposed bandwidth to be used by the session or the media .
         */
        void sdp_add_bandwidth( void ) {}

        /*
         *  Mandatory field: Timing ("t=")
         *  Specify the start and the stop time for a session.
         */
        void sdp_add_timing( void );

        /*
         * Optional field: Time zones ("z=")
         */
        void sdp_add_time_zone( void ) {}

        /*
         * Optional field: Encryption keys ("k=")
         */
        void sdp_add_encryption_key( void ) {}

        /*
         * Optional field: Attributes ("a=")
         */
        void sdp_add_attributes( );

        /*
         * Mandatory field: Media descriptions ("m=")
         */
        void sdp_add_media_description();

        std::string convert_int_to_string (int value);

        void set_remote_ip_from_sdp (const pjmedia_sdp_session *r_sdp);
        
        void set_remote_audio_port_from_sdp (pjmedia_sdp_media *r_media);

        void get_remote_sdp_media_from_offer (const pjmedia_sdp_session* r_sdp, pjmedia_sdp_media** r_media);

	
        /*
         * Adds a sdes attribute to the given media section.
         *
         * @param media The media to add the srtp attribute to 
         */
        void sdp_add_sdes_attribute(std::vector<std::string>& crypto);

        /* 
         * Adds a zrtp-hash  attribute to 
         * the given media section. The hello hash is
         * available only after is has been computed
         * in the AudioZrtpSession constructor. 
         *
         * @param media The media to add the zrtp-hash attribute to 
         * @param hash  The hash to which the attribute should be set to
         */ 
        void sdp_add_zrtp_attribute(pjmedia_sdp_media* media, std::string hash);
              
};


#endif
