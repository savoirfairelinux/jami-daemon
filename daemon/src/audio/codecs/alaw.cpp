/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author:  Yan Morin <yan.morin@savoirfairelinux.com>
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

#include "sfl_types.h"
#include "audiocodec.h"

class Alaw : public sfl::AudioCodec {

    public:
        // 8 PCMA A 8000 1 [RFC3551]
        Alaw() : sfl::AudioCodec(8, "PCMA") {
            clockRate_ = 8000;
            frameSize_ = 160; // samples, 20 ms at 8kHz
            channel_   = 1;
            bitrate_ = 64;
            hasDynamicPayload_ = false;
        }

    private:
        virtual int decode(SFLDataFormat *dst, unsigned char *src, size_t buf_size)
        {
            for (unsigned char* end = src + buf_size; src < end; ++src, ++dst)
                *dst = ALawDecode(*src);

            return frameSize_;
        }

        virtual int encode(unsigned char *dst, SFLDataFormat *src, size_t /*buf_size*/)
        {
            for (unsigned char *end = dst + frameSize_; dst < end; ++src, ++dst)
                *dst = ALawEncode(*src);

            return frameSize_ / 2 /* compression factor = 2:1 */ * sizeof(SFLDataFormat);
        }

        int ALawDecode(uint8 alaw)
        {
            alaw ^= 0x55;  // A-law has alternate bits inverted for transmission
            uint sign = alaw & 0x80;
            int linear = alaw & 0x1f;
            linear <<= 4;
            linear += 8;  // Add a 'half' bit (0x08) to place PCM value in middle of range

            alaw &= 0x7f;

            if (alaw >= 0x20) {
                linear |= 0x100;  // Put in MSB
                uint shift = (alaw >> 4) - 1;
                linear <<= shift;
            }

            if (!sign)
                return -linear;
            else
                return linear;
        }

        uint8 ALawEncode(SFLDataFormat pcm16)
        {
            int p = pcm16;
            uint a;  // u-law value we are forming

            if (p < 0) {
                p = ~p;
                a = 0x00; // sign = 0
            } else {
                //+ve value
                a = 0x80; //sign = 1
            }

            //calculate segment and interval numbers
            p >>= 4;

            if (p >= 0x20) {
                if (p >= 0x100) {
                    p >>= 4;
                    a += 0x40;
                }

                if (p >= 0x40) {
                    p >>= 2;
                    a += 0x20;
                }

                if (p >= 0x20) {
                    p >>= 1;
                    a += 0x10;
                }
            }

            // a&0x70 now holds segment value and 'p' the interval number
            a += p; // a now equal to encoded A-law value

            // A-law has alternate bits inverted for transmission
            return a ^ 0x55;
        }
};

// the class factories
// cppcheck-suppress unusedFunction
extern "C" sfl::Codec* create()
{
    return new Alaw;
}

// cppcheck-suppress unusedFunction
extern "C" void destroy(sfl::Codec* a)
{
    delete a;
}

