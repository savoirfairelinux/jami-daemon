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

#include "sdp.h"
#include "global.h"
#include "manager.h"


static const pj_str_t STR_AUDIO = { (char*)"audio", 5};
static const pj_str_t STR_VIDEO = { (char*)"video", 5};
static const pj_str_t STR_IN = { (char*)"IN", 2 };
static const pj_str_t STR_IP4 = { (char*)"IP4", 3};
static const pj_str_t STR_IP6 = { (char*)"IP6", 3};
static const pj_str_t STR_RTP_AVP = { (char*)"RTP/AVP", 7 };
static const pj_str_t STR_SDP_NAME = { (char*)"sflphone", 7 };
static const pj_str_t STR_SENDRECV = { (char*)"sendrecv", 8 };

    Sdp::Sdp( pj_pool_t *pool, int port ) 
        : _local_media_cap(), _session_media(0),  _ip_addr( "" ), _local_offer( NULL ), _negociated_offer(NULL), _negociator(NULL), _pool(NULL) 
{
    _pool = pool;
    _localAudioPort = 65555; ///port;
}

Sdp::~Sdp() {

    unsigned int k;

    for( k=0; k<_session_media.size(); k++ ){
        delete _session_media[k];
        _session_media[k] = 0;
    }
}

void Sdp::set_media_descriptor_line( sdpMedia *media,
                                  pjmedia_sdp_media** p_med ) {
    pjmedia_sdp_media* med;
    pjmedia_sdp_rtpmap rtpmap;
    pjmedia_sdp_attr *attr;
    AudioCodec *codec;
    int count, i;
    std::ostringstream tmp;

    med = PJ_POOL_ZALLOC_T( _pool, pjmedia_sdp_media );

    // Get the right media format
    pj_strdup(_pool, &med->desc.media,
              ( media->get_media_type() == MIME_TYPE_AUDIO ) ? &STR_AUDIO : &STR_VIDEO );
    med->desc.port_count = 1;
    med->desc.port = 65555; //media->get_port();
    pj_strdup (_pool, &med->desc.transport, &STR_RTP_AVP);

    // Media format ( RTP payload )
    count = media->get_media_codec_list().size();
    med->desc.fmt_count = count;

    // add the payload list
    for(i=0; i<count; i++){
        codec = media->get_media_codec_list()[i];
        tmp << codec->getPayload (); 
        pj_strdup2( _pool, &med->desc.fmt[i], tmp.str().c_str());

        // Add a rtpmap field for each codec
        // We could add one only for dynamic payloads because the codecs with static RTP payloads
        // are entirely defined in the RFC 3351, but if we want to add other attributes like an asymmetric
        // connection, the rtpmap attribute will be useful to specify for which codec it is applicable
        rtpmap.pt = med->desc.fmt[i];
        rtpmap.enc_name = pj_str( (char*) codec->getCodecName().c_str() );
        rtpmap.clock_rate = codec->getClockRate();
        // Add the channel number only if different from 1
        if( codec->getChannel() > 1 )
            rtpmap.param = pj_str( (char*) codec->getChannel() );
        else
            rtpmap.param.slen = 0;
        pjmedia_sdp_rtpmap_to_attr( _pool, &rtpmap, &attr );
        med->attr[med->attr_count++] = attr;
    }
    
    // Add the direction stream
    attr = (pjmedia_sdp_attr*)pj_pool_zalloc( _pool, sizeof(pjmedia_sdp_attr) );
    pj_strdup2( _pool, &attr->name, media->get_stream_direction_str().c_str());
    med->attr[ med->attr_count++] = attr;

    *p_med = med;
}

int Sdp::create_local_offer (){
    pj_status_t status;

    _debug ("Create local offer\n");
    // Build local media capabilities
    set_local_media_capabilities ();

    // Reference: RFC 4566 [5]

    /* Create and initialize basic SDP session */
    this->_local_offer = PJ_POOL_ZALLOC_T(_pool, pjmedia_sdp_session);
    this->_local_offer->conn = PJ_POOL_ZALLOC_T(_pool, pjmedia_sdp_conn);

    /* Initialize the fields of the struct */
    sdp_add_protocol();
    sdp_add_origin();
    sdp_add_session_name();
    sdp_add_connection_info();
    sdp_add_timing();
    //sdp_addAttributes( _pool );
    sdp_add_media_description( );

    _debug ("local port = %i\n", _localAudioPort);
    toString ();

    // Validate the sdp session
    status = pjmedia_sdp_validate( this->_local_offer );
    if (status != PJ_SUCCESS)
        return status;
    
    return PJ_SUCCESS;
}

int Sdp::create_initial_offer(  ){
    pj_status_t status;
    pjmedia_sdp_neg_state state;

    _debug ("Create initial offer\n");
    // Build the SDP session descriptor
    create_local_offer( );

    // Create the SDP negociator instance with local offer
    status = pjmedia_sdp_neg_create_w_local_offer( _pool, get_local_sdp_session(), &_negociator);
    state = pjmedia_sdp_neg_get_state( _negociator );

    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

    return PJ_SUCCESS;
}

int Sdp::receiving_initial_offer( pjmedia_sdp_session* remote ){
    // Create the SDP negociator instance by calling
    // pjmedia_sdp_neg_create_w_remote_offer with the remote offer, and by providing the local offer ( optional )

    pj_status_t status;
    pjmedia_sdp_neg_state state;

    _debug ("Receiving initial offer\n");

    // Create the SDP negociator instance by calling
    // pjmedia_sdp_neg_create_w_remote_offer with the remote offer, and by providing the local offer ( optional )

    // Build the local offer to respond
    create_local_offer(  );

    status = pjmedia_sdp_neg_create_w_remote_offer( _pool,
                                                    get_local_sdp_session(), remote, &_negociator );
    state = pjmedia_sdp_neg_get_state( _negociator );
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

    return PJ_SUCCESS;
}

void Sdp::sdp_add_protocol( void ){
    this->_local_offer->origin.version = 0;
}

void Sdp::sdp_add_origin( void ){
    pj_time_val tv;
    pj_gettimeofday(&tv);

    this->_local_offer->origin.user = pj_str(pj_gethostname()->ptr);
    // Use Network Time Protocol format timestamp to ensure uniqueness.
    this->_local_offer->origin.id = tv.sec + 2208988800UL;
    // The type of network ( IN for INternet )
    this->_local_offer->origin.net_type = STR_IN;
    // The type of address
    this->_local_offer->origin.addr_type = STR_IP4;
    // The address of the machine from which the session was created
    this->_local_offer->origin.addr = pj_str( (char*)_ip_addr.c_str() );
}




void Sdp::sdp_add_session_name( void ){
    this->_local_offer->name = STR_SDP_NAME;
}


void Sdp::sdp_add_connection_info( void ){
    this->_local_offer->conn->net_type = _local_offer->origin.net_type;
    this->_local_offer->conn->addr_type = _local_offer->origin.addr_type;
    this->_local_offer->conn->addr = _local_offer->origin.addr;
}


void Sdp::sdp_add_timing( void ){
    // RFC 3264: An offer/answer model session description protocol
    // As the session is created and destroyed through an external signaling mean (SIP), the line
    // should have a value of "0 0".

    this->_local_offer->time.start = this->_local_offer->time.stop = 0;
}

void Sdp::sdp_add_attributes( ){
    pjmedia_sdp_attr *a;
    this->_local_offer->attr_count = 1;
    a =  PJ_POOL_ZALLOC_T(_pool, pjmedia_sdp_attr);
    a->name=STR_SENDRECV;
    _local_offer->attr[0] = a;
}


void Sdp::sdp_add_media_description( ){
    pjmedia_sdp_media* med;
    int nb_media, i;

    med = PJ_POOL_ZALLOC_T(_pool, pjmedia_sdp_media);
    nb_media = get_local_media_cap().size();
    this->_local_offer->media_count = nb_media;

    for( i=0; i<nb_media; i++ ){
        set_media_descriptor_line( get_local_media_cap()[i], &med );
        this->_local_offer->media[i] = med;
    }
}


std::string Sdp::media_to_string( void ){
    int size, i;
    std::ostringstream res;

    size = _local_media_cap.size();
    for( i = 0; i < size ; i++ ){
        res << _local_media_cap[i]->to_string();
    }

    res << std::endl;
    return res.str();
}

void Sdp::clean_session_media(){
    _session_media.clear();
}

void Sdp::set_negociated_offer( const pjmedia_sdp_session *sdp ){

    int nb_media, nb_codecs;
    int i,j, port;
    pjmedia_sdp_media *current;
    sdpMedia *media;
    std::string type, dir;
    CodecsMap codecs_list;
    CodecsMap::iterator iter;
    AudioCodec *codec_to_add;

    _negociated_offer = (pjmedia_sdp_session*)sdp;
    
    codecs_list = Manager::instance().getCodecDescriptorMap().getCodecsMap();

    // retrieve the media information
    nb_media = _negociated_offer->media_count;
    for( i=0; i<nb_media ; i++ ){
        // Retrieve the media 
        current = _negociated_offer->media[i];
        type = current->desc.media.ptr;
        port = current->desc.port;
        media = new sdpMedia( type, port );
        // Retrieve the payload
        nb_codecs = current->desc.fmt_count;  // Must be one
        for( j=0 ; j<nb_codecs ; j++ ){
            iter = codecs_list.find((AudioCodecType)atoi(current->desc.fmt[j].ptr));  
            if (iter==codecs_list.end())
                return;
            media->add_codec(iter->second);
        }
        _session_media.push_back(media);
    }
}

AudioCodec* Sdp::get_session_media( void ){

    int nb_media;
    int nb_codec;

    nb_media = _session_media.size();
    nb_codec = _session_media[0]->get_media_codec_list().size();

    return _session_media[0]->get_media_codec_list()[0];
}


void Sdp::toString (void) {

    std::ostringstream sdp;

    sdp <<  "origin= " <<  _local_offer->origin.user.ptr << "\n";
    sdp << "origin.id= " << _local_offer->origin.id << "\n";
    sdp << "origin.version= " << _local_offer->origin.version<< "\n";
    sdp << "origin.net_type= " << _local_offer->origin.net_type.ptr<< "\n";
    sdp << "origin.addr_type= " << _local_offer->origin.addr_type.ptr<< "\n";

    sdp << "name=" << _local_offer->name.ptr<< "\n";

    sdp << "conn.net_type=" << _local_offer->conn->net_type.ptr<< "\n";
    sdp << "conn.addr_type=" << _local_offer->conn->addr_type.ptr<< "\n";
    sdp << "conn.addr=" << _local_offer->conn->addr.ptr<< "\n";

    sdp << "start=" <<_local_offer->time.start<< "\n";
    sdp << "stop=" <<_local_offer->time.stop<< "\n";

    sdp << "attr_count=" << _local_offer->attr_count << "\n";
    sdp << "media_count=" << _local_offer->media_count << "\n";
    sdp << "m=" << _local_offer->media[0]->desc.media.ptr << "\n";
    sdp << "port=" << _local_offer->media[0]->desc.port << "\n";
    sdp << "transport=" << _local_offer->media[0]->desc.transport.ptr << "\n";
    sdp << "fmt_count=" << _local_offer->media[0]->desc.fmt_count << "\n";
    sdp << "fmt=" << _local_offer->media[0]->desc.fmt[0].ptr << "\n";
    
    _debug ("LOCAL SDP: \n%s\n", sdp.str().c_str());

}

void Sdp::set_local_media_capabilities () {
    
    CodecOrder selected_codecs;
    int i;
    sdpMedia *audio;
    CodecsMap codecs_list;
    CodecsMap::iterator iter;

    _debug ("Fetch local media capabilities .......... %i" , getLocalExternAudioPort());

    /* Only one audio media used right now */
    audio = new sdpMedia(MIME_TYPE_AUDIO);
    audio->set_port (_localAudioPort);
    
    /* We retrieve the codecs selected by the user */
    selected_codecs = Manager::instance().getCodecDescriptorMap().getActiveCodecs(); 
    codecs_list = Manager::instance().getCodecDescriptorMap().getCodecsMap();
    for (i=0; i<selected_codecs.size(); i++){
        iter = codecs_list.find(selected_codecs[i]);  
    
        if (iter==codecs_list.end())
            return;

        audio->add_codec (iter->second);
    } 
    _local_media_cap.push_back (audio);
    _debug ("%s\n", audio->to_string ().c_str());
}
