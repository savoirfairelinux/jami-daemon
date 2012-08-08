/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 * Author:  Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "global.h"
#include <stdexcept>

static const int G729_PAYLOAD_TYPE = 18;

G729::G729() : sfl::AudioCodec(G729_PAYLOAD_TYPE, "G729", 8000, 160, 1),m_pDecStruct(0),m_pEncStruct(0),
   m_pHandler(0),closeBcg729EncoderChannel(0),bcg729Encoder(0),closeBcg729DecoderChannel(0),bcg729Decoder(0)
{
   init();
}

void G729::init()
{
   m_pHandler = dlopen("/home/lepagee/prefix/lib/libbcg729.so.0.0.0", RTLD_LAZY);
   if (!m_pHandler)
   {
      fprintf(stderr, "%s\n", dlerror());
      throw std::runtime_error("G729 failed to load");
      return;
   }
   closeBcg729EncoderChannel = G729_TYPE_ENCODERCHANNEL dlsym(m_pHandler, "closeBcg729EncoderChannel");
   loadError(dlerror());
   bcg729Encoder             = G729_TYPE_ENCODER        dlsym(m_pHandler, "bcg729Encoder"            );
   loadError(dlerror());
   closeBcg729DecoderChannel = G729_TYPE_DECODERCHANNEL dlsym(m_pHandler, "closeBcg729DecoderChannel");
   loadError(dlerror());
   bcg729Decoder             = G729_TYPE_DECODER        dlsym(m_pHandler, "bcg729Decoder"            );
   loadError(dlerror());

   bcg729DecoderChannelContextStruct*(*decInit)() = G729_TYPE_DECODER_INIT dlsym(m_pHandler, "initBcg729DecoderChannel");
   loadError(dlerror());
   bcg729EncoderChannelContextStruct*(*encInit)() = G729_TYPE_ENCODER_INIT dlsym(m_pHandler, "initBcg729EncoderChannel");
   loadError(dlerror());

   m_pDecStruct = (*decInit)();
   m_pEncStruct = (*encInit)();
}

G729::~G729()
{
   dlclose(m_pHandler);
}

int G729::decode(short *dst, unsigned char *buf, size_t buffer_size UNUSED)
{
   bcg729Decoder(m_pDecStruct,buf,false,dst);
//    bcg729Decoder(m_pDecStruct,buf+(buffer_size/2),false,dst+10);
   return 160;
}

int G729::encode(unsigned char *dst, short *src, size_t buffer_size)
{
   bcg729Encoder(m_pEncStruct,src,dst);
   bcg729Encoder(m_pEncStruct,src+(buffer_size/2),dst+10);
   return 20;
}

void G729::loadError(char* error)
{
   if ((error) != NULL)
   {
      fprintf(stderr, "%s\n", error);
      throw std::runtime_error("G729 failed to load");
   }
}

// cppcheck-suppress unusedFunction
extern "C" sfl::Codec* CODEC_ENTRY()
{
    return new G729;
}

// cppcheck-suppress unusedFunction
extern "C" void destroy(sfl::Codec* a)
{
    delete a;
}
