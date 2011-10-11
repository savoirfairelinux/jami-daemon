/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "audiocodec.h"
#include <cstdio>
#include <celt/celt.h>
#include <stdexcept>


class Celt : public sfl::AudioCodec {

    public:
        Celt(int payload=115)	: sfl::AudioCodec(payload, "celt") {

            _clockRate = 32000;
            _frameSize = 320;  // fixed frameSize, TODO: support variable size from 64 to 512
            _channel = 1;
            _bitrate = 0;
            _hasDynamicPayload = true;

            int error = 0;

            _mode = celt_mode_create(_clockRate, _frameSize, &error);

            if (error != CELT_OK) {
                switch (error) {
                    case CELT_BAD_ARG:
                        throw std::runtime_error("Celt: An (or more) invalid argument (e.g. out of range)\n");
                        break;
                    case CELT_INVALID_MODE:
                        throw std::runtime_error("Celt: The mode struct passed is invalid\n");
                        break;
                    case CELT_INTERNAL_ERROR:
                        throw std::runtime_error("Celt: An internal error was detected\n");
                        break;
                    case CELT_CORRUPTED_DATA:
                        throw std::runtime_error("Celt: The data passed (e.g. compressed data to decoder) is corrupted\n");
                        break;
                    case CELT_UNIMPLEMENTED:
                        throw std::runtime_error("Celt: Invalid/unsupported request numbe\n");
                        break;
                    case CELT_INVALID_STATE:
                        throw std::runtime_error("Celt: An encoder or decoder structure is invalid or already freed\n");
                        break;
                    case CELT_ALLOC_FAIL:
                        throw std::runtime_error("Celt: Memory allocation has failed\n");
                        break;
                    default:
                        throw std::runtime_error("Celt: Unknown error");
                }

            }

            if (_mode == NULL)
                throw std::runtime_error("Celt: Failed to create Celt mode");

            // bytes_per_packet = 1024;
            // if (bytes_per_packet < 0 || bytes_per_packet > MAX_PACKET)
            // {
            //     throw std::runtime_error("bytes per packet must be between 0 and %d");
            // }

            // celt_mode_info(mode, CELT_GET_FRAME_SIZE, &frame_size);
            // celt_mode_info(mode, CELT_GET_NB_CHANNELS, &_channel);

            _enc = celt_encoder_create(_mode, _channel, &error);

            _dec = celt_decoder_create(_mode, _channel, &error);

            celt_encoder_ctl(_enc, CELT_SET_COMPLEXITY(2));
            celt_decoder_ctl(_dec, CELT_SET_COMPLEXITY(2));

            celt_encoder_ctl(_enc, CELT_SET_PREDICTION(2));
            celt_decoder_ctl(_dec, CELT_SET_PREDICTION(2));

        }

        Celt(const Celt&);
        Celt& operator= (const Celt&);

        ~Celt() {
            celt_encoder_destroy(_enc);
            celt_decoder_destroy(_dec);
            celt_mode_destroy(_mode);
        }

        virtual int decode(short *dst, unsigned char *src, size_t buf_size) {
#ifdef BUILD_CELT_91 // == 91
            //int err = 0;
            /*err =*/ celt_decode(_dec, src, buf_size, (celt_int16*) dst, _frameSize);
#endif
#ifdef BUILD_CELT_71
            //int err = 0; // FIXME: check error code
            /*err =*/ celt_decode(_dec, src, buf_size, (celt_int16*) dst);
#endif
            return _frameSize;
        }

        virtual int encode(unsigned char *dst, short *src, size_t buf_size) {
            int len = 0;
#ifdef BUILD_CELT_91// == 91
            len = celt_encode(_enc, (celt_int16*) src, _frameSize, dst, buf_size);
#endif
#ifdef BUILD_CELT_71
            len = celt_encode(_enc, (celt_int16*) src, (celt_int16 *) src, dst, buf_size);
#endif
            return len;
        }

    private:

        CELTMode *_mode;

        CELTEncoder *_enc;
        CELTDecoder *_dec;

        celt_int32 _celt_frame_size;
        celt_int32 skip;

};

// the class factories
extern "C" sfl::Codec* create()
{
    return new Celt(115);
}

extern "C" void destroy(sfl::Codec* a)
{
    delete a;
}
