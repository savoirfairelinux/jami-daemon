/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
extern "C"
{
#include <gsm/gsm.h>
}

/**
 * GSM audio codec C++ class (over gsm/gsm.h)
 */

class Gsm : public AudioCodec
{

    public:
        // _payload should be 3
        Gsm (int payload=3) : AudioCodec (payload, "GSM"), _decode_gsmhandle (NULL), _encode_gsmhandle (NULL) {
            _clockRate = 8000;
            _frameSize = 160; // samples, 20 ms at 8kHz
            _channel = 1;
            _bitrate = 13.3;
            _bandwidth = 29.2;

            if (! (_decode_gsmhandle = gsm_create()))
                printf ("ERROR: decode_gsm_create");

            if (! (_encode_gsmhandle = gsm_create()))
                printf ("AudioCodec: ERROR: encode_gsm_create");
        }

        Gsm (const Gsm&);

        Gsm& operator= (const Gsm&);

        virtual ~Gsm (void) {
            gsm_destroy (_decode_gsmhandle);
            gsm_destroy (_encode_gsmhandle);
        }

        virtual int	codecDecode	(short * dst, unsigned char * src, unsigned int size) {
            // _debug("Decoded by gsm ");
            (void) size;

            if (gsm_decode (_decode_gsmhandle, (gsm_byte*) src, (gsm_signal*) dst) < 0)
                printf ("ERROR: gsm_decode");

            return 320;
        }

        virtual int	codecEncode	(unsigned char * dst, short * src, unsigned int size) {

            // _debug("Encoded by gsm ");
            (void) size;
            gsm_encode (_encode_gsmhandle, (gsm_signal*) src, (gsm_byte*) dst);
            return 33;
        }

    private:
        gsm _decode_gsmhandle;
        gsm _encode_gsmhandle;
};

extern "C" AudioCodec* create()
{
    return new Gsm (3);
}

extern "C" void destroy (AudioCodec* a)
{
    delete a;
}
