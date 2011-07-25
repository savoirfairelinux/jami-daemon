/*
 * Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 * Author:  Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 * Mostly borrowed from asterisk's sources (Steve Underwood <steveu@coppice.org>)
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
#ifndef __AUDIO_CODEC_H__
#define __AUDIO_CODEC_H__

#include <string>
#include <iostream>
#include <dlfcn.h>

#include "Codec.h"

/* bump when codec binary interface changes */
#define CODEC_ENTRY create_0_9_14
#define CODEC_ENTRY_SYMBOL "create_0_9_14"

namespace ost {
    class PayloadFormat;
    class DynamicPayloadFormat;
}

namespace sfl {

class AudioCodec : public Codec
{
    public:
        AudioCodec (uint8 payload, const std::string &codecName);

        /**
         * Copy constructor.
         */
        AudioCodec (const AudioCodec& codec);

        virtual ~AudioCodec() {}

        /**
         * @Override
         */
        std::string getMimeType() const;

        /**
         * @Override
         */
        std::string getMimeSubtype() const;

        /**
         * @Override
         */
        const ost::PayloadFormat& getPayloadFormat();

        /**
         * @Override
         */
        void setParameter (const std::string& /*name*/, const std::string& /*value*/) {};

        /**
         * @Override
         */
        std::string getParameter (const std::string& /*name*/) const {
            return "";
        };

        /**
         * Decode an input buffer and fill the output buffer with the decoded data
         * @return the number of bytes decoded
         */
        virtual int decode (short *, unsigned char *, unsigned int) = 0;

        /**
         * Encode an input buffer and fill the output buffer with the encoded data
         * @return the number of bytes encoded
         */
        virtual int encode (unsigned char *, short *, unsigned int) = 0;

        /**
         * @Override
         */
        uint8 getPayloadType() const ;

        /**
         * @Override
         */
        void setPayloadType(uint8 pt) {
        	_payload = pt;
        }

        /**
         * @return true if this payload is a dynamic one.
         */
        bool hasDynamicPayload() const;

        /**
         * @Override
         */
        uint32 getClockRate() const;

        /**
         * @return the number of audio channels.
         */
        uint8 getChannel() const;

        /**
         * @Override
         */
        double getBitRate() const;

        /**
         * @Override
         */
        double getBandwidth() const;

        /**
         * @return the framing size for this codec.
         */
        unsigned int getFrameSize() const;

    protected:
        /** Holds SDP-compliant codec name */
        std::string _codecName; // what we put inside sdp

        /** Clock rate or sample rate of the codec, in Hz */
        uint32 _clockRate;

        /** Number of channel 1 = mono, 2 = stereo */
        uint8 _channel;

        /** codec frame size in samples*/
        unsigned _frameSize;

        /** Bitrate */
        double _bitrate;

        /** Bandwidth */
        double _bandwidth;

        bool _hasDynamicPayload;

        void setCodecName (const std::string& codecName) {
            _codecName = codecName;
        }

        void setClockRate (uint32 rate) {
            _clockRate = rate;
        }

        void setChannel (uint8 channel) {
            _channel = channel;
        }

        void setFrameSize (unsigned size) {
            _frameSize = size;
        }

        void setBitrate (double rate) {
            _bitrate = rate;
        }

        void setBandwidth (double bandwidth) {
            _bandwidth = bandwidth;
        }
    private:
        uint8 _payload;

        ost::DynamicPayloadFormat* _payloadFormat;

        void init (uint8 payloadType, uint32 clockRate);
};
} // end namespace sfl

#endif
