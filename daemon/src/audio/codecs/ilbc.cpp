/*
 *  Copyright (C) 2012-2013 Savoir-Faire Linux Inc.
 *  Author:  Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
#include <algorithm>

extern "C" {
#include <ilbc.h>
}

class Ilbc: public sfl::AudioCodec {
    public:
        Ilbc() :
            sfl::AudioCodec(ILBC_PAYLOAD, "iLBC", 8000, ILBC_FRAME_SIZE, 1),
            ilbc_dec_(),
            ilbc_enc_() {
            bitrate_ = 13.3;

            initDecode(&ilbc_dec_, 20, 1);
            initEncode(&ilbc_enc_, 20);
        }

    private:
        AudioCodec *
        clone()
        {
            return new Ilbc;
        }

        // iLBC expects floating point data, so we have to convert
        int decode(SFLAudioSample *pcm, unsigned char *data, size_t) {
            const int NORMAL_MODE = 1;
            iLBC_decode(pcm, reinterpret_cast<WebRtc_UWord16*>(data), &ilbc_dec_, NORMAL_MODE);
            return frameSize_;
        }

        int encode(unsigned char *data, SFLAudioSample *pcm, size_t) {
            iLBC_encode(reinterpret_cast<WebRtc_UWord16*>(data), pcm, &ilbc_enc_);
            return frameSize_;
        }

        static const int ILBC_FRAME_SIZE = 160;
        static const int ILBC_PAYLOAD = 105;
        iLBC_Dec_Inst_t ilbc_dec_;
        iLBC_Enc_Inst_t ilbc_enc_;
};

// the class factories
extern "C" sfl::AudioCodec* AUDIO_CODEC_ENTRY()
{
    return new Ilbc;
}

extern "C" void destroy(sfl::AudioCodec* a)
{
    delete a;
}
