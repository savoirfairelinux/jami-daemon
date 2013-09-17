/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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
#include "sfl_types.h"

class Ulaw : public sfl::AudioCodec {
    public:
        // 0 PCMU A 8000 1 [RFC3551]
        Ulaw() : sfl::AudioCodec(0, "PCMU", 8000, 160, 1) {
            bitrate_ =  64;
            hasDynamicPayload_ = false;
        }

    private:
        int decode(SFLAudioSample *dst, unsigned char *src, size_t buf_size) {
            for (unsigned char* end = src + buf_size; src < end; ++src, ++dst)
                *dst = ULawDecode(*src);

            return buf_size;
        }

        int encode(unsigned char *dst, SFLAudioSample *src, size_t buf_size) {
            for (unsigned char * end = dst + buf_size; dst < end; ++src, ++dst)
                *dst = ULawEncode(*src);

            return buf_size;
        }

        SFLAudioSample ULawDecode(uint8_t ulaw) {
            ulaw ^= 0xff;  // u-law has all bits inverted for transmission
            int linear = ulaw & 0x0f;
            linear <<= 3;
            linear |= 0x84;  // Set MSB (0x80) and a 'half' bit (0x04) to place PCM value in middle of range

            uint8_t shift = ulaw >> 4;
            shift &= 7;
            linear <<= shift;
            linear -= 0x84; // Subract uLaw bias

            if (ulaw & 0x80)
                return -linear;
            else
                return linear;
        }

        uint8_t ULawEncode(SFLAudioSample pcm16) {
            int p = pcm16;
            uint8_t u;  // u-law value we are forming

            if (p < 0) {
                p = ~p;
                u = 0x80 ^ 0x10 ^ 0xff;  // Sign bit = 1 (^0x10 because this will get inverted later) ^0xff ^0xff to invert final u-Law code
            } else {
                u = 0x00 ^ 0x10 ^ 0xff;  // Sign bit = 0 (-0x10 because this amount extra will get added later) ^0xff to invert final u-Law code
            }

            p += 0x84; // Add uLaw bias

            if (p > 0x7f00)
                p = 0x7f00;  // Clip to 15 bits

            // Calculate segment and interval numbers
            p >>= 3;        // Shift down to 13bit

            if (p >= 0x100) {
                p >>= 4;
                u ^= 0x40;
            }

            if (p >= 0x40) {
                p >>= 2;
                u ^= 0x20;
            }

            if (p >= 0x20) {
                p >>= 1;
                u ^= 0x10;
            }

            u ^= p; // u now equal to encoded u-law value (with all bits inverted)

            return u;
        }
};

// the class factories
// cppcheck-suppress unusedFunction
extern "C" sfl::AudioCodec* AUDIO_CODEC_ENTRY()
{
    return new Ulaw;
}

// cppcheck-suppress unusedFunction
extern "C" void destroy(sfl::AudioCodec* a)
{
    delete a;
}
