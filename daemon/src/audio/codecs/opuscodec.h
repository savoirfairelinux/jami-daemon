/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author:  Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>
 *  Author: Adrien Beraud <adrien.beraud@wisdomvibes.com>
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
#ifndef OPUS_H_
#define OPUS_H_

#include "audiocodec.h"

#include "noncopyable.h"
#include "sfl_types.h"

#include <opus.h>

class Opus : public sfl::AudioCodec {
public:
   Opus();
   ~Opus();

   static const uint8_t PAYLOAD_TYPE = 104; // dynamic payload type, out of range of video (96-99)

   virtual inline bool supportsPacketLossConcealment() const {
      return true;
   }

private:
   sfl::AudioCodec * clone();

   virtual int decode(std::vector<std::vector<SFLAudioSample> > &pcm, const uint8_t *data, size_t len);
   virtual int decode(std::vector<std::vector<SFLAudioSample> > &pcm);

   virtual size_t encode(const std::vector<std::vector<SFLAudioSample> > &pcm, uint8_t *data, size_t len);

   virtual uint32_t getSDPClockRate() const;
   virtual const char *getSDPChannels() const;

   virtual void setOptimalFormat(uint32_t sample_rate, uint8_t channels);

   NON_COPYABLE(Opus);
   //Attributes
   OpusEncoder *encoder_;
   OpusDecoder *decoder_;

   unsigned lastDecodedFrameSize_;

   // Valid sampling rates allowed by the Opus library.
   static constexpr uint32_t VALID_SAMPLING_RATE[] = {8000, 12000, 16000, 24000, 48000};
   static constexpr size_t VALID_SAMPLING_RATE_NUM = sizeof(VALID_SAMPLING_RATE)/sizeof(uint32_t);

   static const unsigned CLOCK_RATE = 48000;
   static const unsigned FRAME_SIZE = 20 * CLOCK_RATE / 1000; // 20ms
   static const unsigned CHANNELS   = 2;

   // Opus documentation:
   // "If this is less than the maximum packet duration (120ms; 5760 for 48kHz),
   // opus_decode will not be capable of decoding some packets."
   static const unsigned MAX_PACKET_SIZE = 120 * CLOCK_RATE / 1000; // 120ms
};

#endif
