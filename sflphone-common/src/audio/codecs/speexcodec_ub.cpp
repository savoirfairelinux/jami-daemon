/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include "audiocodec.h"
#include <cstdio>
#include <speex/speex.h>

class Speex : public AudioCodec
{

    public:
        Speex (int payload=0)
                : AudioCodec (payload, "speex"),
                _speexModePtr (NULL),
                _speex_dec_bits(),
                _speex_enc_bits(),
                _speex_dec_state(),
                _speex_enc_state(),
		_speex_frame_size() {
            _clockRate = 32000;
            _frameSize = 640; // 20 ms at 32 kHz
            _channel = 1;
            _bitrate = 0;
            _bandwidth = 0;
            initSpeex();
        }

        Speex (const Speex&);
        Speex& operator= (const Speex&);

        void initSpeex() {

            // int _samplingRate = 32000;

            // 8000 HZ --> Narrow-band mode
            // TODO Manage the other modes
            _speexModePtr = &speex_wb_mode;
            // _speexModePtr = &speex_wb_mode;

            // Init the decoder struct
            speex_bits_init (&_speex_dec_bits);
            _speex_dec_state = speex_decoder_init (_speexModePtr);

            // Init the encoder struct
            speex_bits_init (&_speex_enc_bits);
            _speex_enc_state = speex_encoder_init (_speexModePtr);

            speex_encoder_ctl (_speex_enc_state,SPEEX_SET_SAMPLING_RATE,&_clockRate);

            speex_decoder_ctl (_speex_dec_state, SPEEX_GET_FRAME_SIZE, &_speex_frame_size);
        }

        ~Speex() {
            terminateSpeex();
        }

        void terminateSpeex() {
            // Destroy the decoder struct
            speex_bits_destroy (&_speex_dec_bits);
            speex_decoder_destroy (_speex_dec_state);
            _speex_dec_state = 0;

            // Destroy the encoder struct
            speex_bits_destroy (&_speex_enc_bits);
            speex_encoder_destroy (_speex_enc_state);
            _speex_enc_state = 0;

        }

        virtual int codecDecode (short *dst, unsigned char *src, unsigned int size) {

            // int ratio = 320 / _speex_frame_size;
            speex_bits_read_from (&_speex_dec_bits, (char*) src, size);
            speex_decode_int (_speex_dec_state, &_speex_dec_bits, dst);

            // return the nuber of byte, not the number of sample
            return _frameSize * 2;
        }

        virtual int codecEncode (unsigned char *dst, short *src, unsigned int size) {
            speex_bits_reset (&_speex_enc_bits);

            speex_encode_int (_speex_enc_state, src, &_speex_enc_bits);
            speex_bits_nbytes (&_speex_enc_bits);
            int nbBytes = speex_bits_write (&_speex_enc_bits, (char*) dst, size);

            return nbBytes;
        }

    private:
        const SpeexMode* _speexModePtr;
        SpeexBits  _speex_dec_bits;
        SpeexBits  _speex_enc_bits;
        void *_speex_dec_state;
        void *_speex_enc_state;
        int _speex_frame_size;
};

// the class factories
extern "C" AudioCodec* create()
{
    return new Speex (112);
}

extern "C" void destroy (AudioCodec* a)
{
    delete a;
}

