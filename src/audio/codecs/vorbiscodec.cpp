/*
 *  Copyright (C) 2007-2009 Savoir-Faire Linux inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "audiocodec.h"
#include <cstdio>
#include <math.h>
#include <vorbis/vorbis.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>

class Vorbis : public AudioCodec{
    public:
        Vorbis(int payload=0)
            : AudioCodec(payload, "vorbis"),
              _ogg_stream_state(),
              _ogg_packet(),
              _vorbis_info(),
              _vorbis_comment(),
              _vorbis_dsp_state(),
              _vorbis_block()
    {
        _clockRate = 8000;
        _channel = 1;
        _bitrate = 0;
        _bandwidth = 0; 
        initVorbis();
    }

        Vorbis( const Vorbis& );
        Vorbis& operator=(const Vorbis&);

        void initVorbis() { 

            // init the encoder
            vorbis_info_init(&_vorbis_info); 
            vorbis_encode_init_vbr(&_vorbis_info,0.5);

            vorbis_comment_init(&_vorbis_comment);

            vorbis_analysis_init(&_vorbis_dsp_state, &_vorbis_info);

            // random number for ogg serial number
            srand(time(NULL));

        }

        ~Vorbis() 
        {
            terminateVorbis();
        }

        void terminateVorbis() {

            vorbis_block_clear(&_vorbis_block);
            vorbis_dsp_clear(&_vorbis_dsp_state);
            vorbis_comment_clear(&_vorbis_comment);
            vorbis_info_clear(&_vorbis_info);
        }

        virtual int codecDecode (short *dst, unsigned char *src, unsigned int size) 
        {
            

            return 1;
        }

        virtual int codecEncode (unsigned char *dst, short *src, unsigned int size) 
        {
           
            return 1;
        }

    private:

        // ogg-vorbis specific variables
	ogg_sync_state oy;

	ogg_stream_state _ogg_stream_state;

	ogg_packet _ogg_packet;

	vorbis_info _vorbis_info;

	vorbis_comment _vorbis_comment;

        vorbis_dsp_state _vorbis_dsp_state;

	vorbis_block _vorbis_block;

        
};

// the class factories
extern "C" AudioCodec* create() {
    return new Vorbis(117);
}

extern "C" void destroy(AudioCodec* a) {
    delete a;
}

