/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author:  Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
#include <cassert>
#include <stdexcept>

extern "C" {
#include <gsm/gsm.h>
}

/**
 * GSM audio codec C++ class (over gsm/gsm.h)
 */

class Gsm : public sfl::AudioCodec
{

    public:
        // _payload should be 3
        Gsm (int payload=3) : sfl::AudioCodec (payload, "GSM"), _decode_gsmhandle (NULL), _encode_gsmhandle (NULL) {
            _clockRate = 8000;
            _frameSize = 160; // samples, 20 ms at 8kHz
            _channel = 1;
            _bitrate = 13.3;
            _hasDynamicPayload = false;

            if (! (_decode_gsmhandle = gsm_create()))
                throw std::runtime_error("ERROR: decode_gsm_create\n");

            if (! (_encode_gsmhandle = gsm_create()))
                throw std::runtime_error("ERROR: encode_gsm_create\n");
        }

        Gsm (const Gsm&);

        Gsm& operator= (const Gsm&);

        virtual ~Gsm (void) {
            gsm_destroy (_decode_gsmhandle);
            gsm_destroy (_encode_gsmhandle);
        }

        virtual int	decode	(short * dst, unsigned char * src, size_t buf_size) {
        	assert(buf_size == 33);
        	(void) buf_size;

            if (gsm_decode (_decode_gsmhandle, (gsm_byte*) src, (gsm_signal*) dst) < 0)
                throw std::runtime_error("ERROR: gsm_decode\n");

            return _frameSize;
        }

        virtual int	encode	(unsigned char * dst, short * src, size_t buf_size) {
            (void) buf_size;
            assert(buf_size >= 33);
            gsm_encode (_encode_gsmhandle, (gsm_signal*) src, (gsm_byte*) dst);
            return 33;
        }

    private:
        gsm _decode_gsmhandle;
        gsm _encode_gsmhandle;
};

extern "C" sfl::Codec* create()
{
    return new Gsm (3);
}

extern "C" void destroy (sfl::Codec* a)
{
    delete a;
}
