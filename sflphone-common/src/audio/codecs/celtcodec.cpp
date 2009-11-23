/*
 *  Copyright (C) 2007-2009 Savoir-Faire Linux inc.
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
 */

#include "audiocodec.h"
#include <cstdio>
#include <celt/celt.h>


class Celt : public AudioCodec
{

    public:
        Celt (int payload=0)
                : AudioCodec (payload, "celt") {
            _clockRate = 44100;
            _frameSize = 512; // fixed frameSize, TODO: support variable size from 64 to 512
            _channel = 1;
            _bitrate = 0;
            _bandwidth = 0;
            initCelt();
        }

        Celt (const Celt&);
        Celt& operator= (const Celt&);

        void initCelt() {
            printf ("init celt");

            mode = celt_mode_create (_clockRate, _channel, _frameSize, NULL);
            // celt_mode_info(mode, CELT_GET_LOOKAHEAD, &skip);

            if (mode == NULL) {
                printf ("failed to create a mode");
            }

            // bytes_per_packet = 1024;
            // if (bytes_per_packet < 0 || bytes_per_packet > MAX_PACKET)
            // {
            //     printf ("bytes per packet must be between 0 and %d");
            // }

            // celt_mode_info(mode, CELT_GET_FRAME_SIZE, &frame_size);
            // celt_mode_info(mode, CELT_GET_NB_CHANNELS, &_channel);

            enc = celt_encoder_create (mode);

            dec = celt_decoder_create (mode);

            celt_encoder_ctl (enc,CELT_SET_COMPLEXITY (10));

        }

        ~Celt() {
            terminateCelt();
        }

        void terminateCelt() {

            celt_encoder_destroy (enc);
            celt_decoder_destroy (dec);
        }

        virtual int codecDecode (short *dst, unsigned char *src, unsigned int size) {
            int err = 0;
            err = celt_decode (dec, src, size, (celt_int16_t*) dst);
            return _frameSize * sizeof (celt_int16_t);
        }

        virtual int codecEncode (unsigned char *dst, short *src, unsigned int size) {
            int len = 0;
            len = celt_encode (enc, (celt_int16_t *) src, (celt_int16_t *) src, dst, 512);
            // returns the number of bytes writen
            return len;
        }

    private:

        CELTMode *mode;

        CELTEncoder *enc;
        CELTDecoder *dec;

        celt_int32_t _celt_frame_size;
        celt_int32_t skip;

};

// the class factories
extern "C" AudioCodec* create()
{
    return new Celt (115);
}

extern "C" void destroy (AudioCodec* a)
{
    delete a;
}
