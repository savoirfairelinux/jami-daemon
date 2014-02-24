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

#include "sfl_types.h"
#include "audiocodec.h"
#include "g711.h"

class Alaw : public sfl::AudioCodec {

    public:
        // 8 PCMA A 8000 1 [RFC3551]
        Alaw() : sfl::AudioCodec(8, "PCMA", 8000, 160, 1) {
            bitrate_ = 64;
            hasDynamicPayload_ = false;
        }

    private:
        AudioCodec *
        clone()
        {
            return new Alaw;
        }

        int decode(SFLAudioSample *dst, unsigned char *src, size_t buf_size)
        {
            for (unsigned char* end = src + buf_size; src < end; ++src, ++dst)
                *dst = ALawDecode(*src);

            return buf_size;
        }

        int encode(unsigned char *dst, SFLAudioSample *src, size_t /* buf_size */)
        {
            for (unsigned char *end = dst + frameSize_; dst < end; ++src, ++dst)
                *dst = ALawEncode(*src);

            return frameSize_;
        }

        static int ALawDecode(uint8_t alaw) {
            return alaw_to_linear(alaw);
        }

        static uint8_t ALawEncode(SFLAudioSample pcm16) {
            return linear_to_alaw(pcm16);
        }
};

// the class factories
// cppcheck-suppress unusedFunction
extern "C" sfl::AudioCodec* AUDIO_CODEC_ENTRY()
{
    return new Alaw;
}

// cppcheck-suppress unusedFunction
extern "C" void destroy(sfl::AudioCodec* a)
{
    delete a;
}

