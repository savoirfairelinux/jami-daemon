/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author:  Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>
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
#include "g729.h"
#include "sfl_types.h"
#include <iostream>
#include <dlfcn.h>
#include <stdexcept>

#define G729_TYPE_ENCODER        (void (*)(bcg729EncoderChannelContextStruct*, SFLAudioSample[], uint8_t[]))
#define G729_TYPE_DECODER        (void (*)(bcg729DecoderChannelContextStruct*, uint8_t[], uint8_t, SFLAudioSample[]))

#define G729_TYPE_DECODER_INIT   (bcg729DecoderChannelContextStruct*(*)())
#define G729_TYPE_ENCODER_INIT   (bcg729EncoderChannelContextStruct*(*)())

static const int G729_PAYLOAD_TYPE = 18;

G729::G729() : sfl::AudioCodec(G729_PAYLOAD_TYPE, "G729", 8000, 160, 1),
    decoderContext_(0),
    encoderContext_(0),
    handler_(0),
    encoder_(0),
    decoder_(0)
{
    handler_ = dlopen("libbcg729.so.0", RTLD_NOW);

    if (!handler_)
        throw std::runtime_error("g729: did not open shared lib");

    encoder_ = G729_TYPE_ENCODER dlsym(handler_, "bcg729Encoder");
    loadError(dlerror());
    decoder_ = G729_TYPE_DECODER dlsym(handler_, "bcg729Decoder");
    loadError(dlerror());

    bcg729DecoderChannelContextStruct*(*decInit)() = G729_TYPE_DECODER_INIT dlsym(handler_, "initBcg729DecoderChannel");
    loadError(dlerror());
    bcg729EncoderChannelContextStruct*(*encInit)() = G729_TYPE_ENCODER_INIT dlsym(handler_, "initBcg729EncoderChannel");
    loadError(dlerror());

    decoderContext_ = (*decInit)();
    encoderContext_ = (*encInit)();
}

G729::~G729()
{
    if (handler_)
        dlclose(handler_);
}

sfl::AudioCodec *
G729::clone()
{
    return new G729;
}

int G729::decode(SFLAudioSample *pcm, unsigned char *data, size_t len)
{
    decoder_(decoderContext_, data, false, pcm);
    decoder_(decoderContext_, data + (len / 2), false, pcm + 80);
    return 160;
}

int G729::encode(unsigned char *data, SFLAudioSample *pcm, size_t)
{
    encoder_(encoderContext_, pcm, data);
    encoder_(encoderContext_, pcm + (frameSize_ / 2), data + 10);
    return 20;
}

void G729::loadError(const char *error)
{
    if (error != NULL)
        throw std::runtime_error("G729 failed to load");
}

// cppcheck-suppress unusedFunction
extern "C" sfl::AudioCodec* AUDIO_CODEC_ENTRY()
{
    try {
        return new G729;
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
