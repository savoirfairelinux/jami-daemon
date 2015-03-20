/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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
#include "ring_types.h"
#include "ring_plugin.h"
#include "g711.h"

class Ulaw : public ring::AudioCodec {
    public:
        // 0 PCMU A 8000 1 [RFC3551]
        Ulaw() : ring::AudioCodec(0, "PCMU", 8000, 160, 1) {
            bitrate_ =  64;
            hasDynamicPayload_ = false;
        }

    private:
        AudioCodec *
        clone()
        {
            return new Ulaw;
        }

        int decode(ring::AudioSample *pcm, unsigned char *data, size_t len)
        {
            for (const unsigned char *end = data + len; data < end;
                 ++data, ++pcm)
                *pcm = ULawDecode(*data);

            return len;
        }

        int encode(unsigned char *data, ring::AudioSample *pcm, size_t max_data_bytes)
        {
            const unsigned char *end = data +
                std::min<size_t>(frameSize_, max_data_bytes);

            unsigned char *tmp = data;
            for (; tmp < end; ++tmp, ++pcm)
                *tmp = ULawEncode(*pcm);

            return end - data;
        }

        static ring::AudioSample ULawDecode(uint8_t ulaw) {
            return ulaw_to_linear(ulaw);
        }

        static uint8_t ULawEncode(ring::AudioSample pcm16) {
            return linear_to_ulaw(pcm16);
        }
};

// the class factories
// cppcheck-suppress unusedFunction
RING_PLUGIN_EXIT(pluginExit) {}

// cppcheck-suppress unusedFunction
RING_PLUGIN_INIT_DYNAMIC(pluginAPI)
{
    std::unique_ptr<Ulaw> codec(new Ulaw);

    if (!pluginAPI->invokeService(pluginAPI, "registerAudioCodec",
                                  reinterpret_cast<void*>(codec.get()))) {
        codec.release();
        return pluginExit;
    }

    return nullptr;
}
