/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include "noncopyable.h"
#include <stdexcept>
#include <iostream>

// FIXME: this should be set by configure
extern "C" {
#ifdef __ANDROID__
#include <gsm.h>
#else
#include <gsm/gsm.h>
#endif
}

/**
 * GSM audio codec C++ class (over gsm/gsm.h)
 */

class Gsm : public sfl::AudioCodec {

    public:
        // _payload should be 3
        Gsm() : sfl::AudioCodec(3, "GSM", 8000, 160, 1),
            decode_gsmhandle_(NULL), encode_gsmhandle_(NULL) {
            bitrate_ = 13.3;
            hasDynamicPayload_ = false;

            if (!(decode_gsmhandle_ = gsm_create()))
                throw std::runtime_error("ERROR: decode_gsm_create\n");

            if (!(encode_gsmhandle_ = gsm_create()))
                throw std::runtime_error("ERROR: encode_gsm_create\n");
        }

        ~Gsm() {
            gsm_destroy(decode_gsmhandle_);
            gsm_destroy(encode_gsmhandle_);
        }
private:
        AudioCodec *
        clone()
        {
            return new Gsm;
        }

        int decode(SFLAudioSample *pcm, unsigned char *data, size_t)
        {
            if (gsm_decode(decode_gsmhandle_, (gsm_byte*) data, (gsm_signal*) pcm) < 0)
                throw std::runtime_error("ERROR: gsm_decode\n");

            return frameSize_;
        }

        int encode(unsigned char *data, SFLAudioSample *pcm, size_t)
        {
            gsm_encode(encode_gsmhandle_, (gsm_signal*) pcm, (gsm_byte*) data);
            return sizeof(gsm_frame);
        }

        NON_COPYABLE(Gsm);
        gsm decode_gsmhandle_;
        gsm encode_gsmhandle_;
};

// cppcheck-suppress unusedFunction
extern "C" sfl::AudioCodec* AUDIO_CODEC_ENTRY()
{
    try {
        return new Gsm;
    } catch (const std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        return 0;
    }
}

// cppcheck-suppress unusedFunction
extern "C" void destroy(sfl::AudioCodec* a)
{
    delete a;
}
