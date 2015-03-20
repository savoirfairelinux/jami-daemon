/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
#ifndef IAXCALL_H
#define IAXCALL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "call.h"
#include "noncopyable.h"

class iax_session;

namespace ring {

class IAXAccount;
class RingBuffer;
class AudioBuffer;

/** Enumeration that contains known audio payloads */
enum {
    // http://www.iana.org/assignments/rtp-parameters
    // http://www.gnu.org/software/ccrtp/doc/refman/html/formats_8h.html#a0
    // 0 PCMU A 8000 1 [RFC3551]
    PAYLOAD_CODEC_ULAW = 0,
    // 3 GSM  A 8000 1 [RFC3551]
    PAYLOAD_CODEC_GSM = 3,
    // 8 PCMA A 8000 1 [RFC3551]
    PAYLOAD_CODEC_ALAW = 8,
    // 9 G722 A 8000 1 [RFC3551]
    PAYLOAD_CODEC_G722 = 9,
    // http://www.ietf.org/rfc/rfc3952.txt
    // 97 iLBC/8000
    PAYLOAD_CODEC_ILBC_20 = 97,
    PAYLOAD_CODEC_ILBC_30 = 98,
    // http://www.speex.org/drafts/draft-herlein-speex-rtp-profile-00.txt
    //  97 speex/8000
    // http://support.xten.com/viewtopic.php?p=8684&sid=3367a83d01fdcad16c7459a79859b08e
    // 100 speex/16000
    PAYLOAD_CODEC_SPEEX_8000 = 110,
    PAYLOAD_CODEC_SPEEX_16000 = 111,
    PAYLOAD_CODEC_SPEEX_32000 = 112
};


/**
 * @file: iaxcall.h
 * @brief IAXCall are IAX implementation of a normal Call
 */

class IAXCall : public Call
{
    public:
        static const char* const LINK_TYPE;

    protected:
        /**
         * Constructor
         * @param id  The unique ID of the call
         * @param type  The type of the call
         */
        IAXCall(IAXAccount& account, const std::string& id, Call::CallType type);

    public:
        /**
         * @return int  The bitwise list of supported formats
         */
        int getSupportedFormat(const std::string &accountID) const;

        /**
         * Return a format (int) with the first matching codec selected.
         *
         * This considers the order of the appearance in the CodecMap,
         * thus, the order of preference.
         *
         * NOTE: Everything returned is bound to the content of the local
         *       CodecMap, so it won't return format values that aren't valid
         *       in this call context.
         *
         * @param needles  The format(s) (bitwise) you are looking for to match
         * @return int  The matching format, thus 0 if none matches
         */
        int getFirstMatchingFormat(int needles, const std::string &accountID) const;

        int getAudioCodecPayload() const;

        int format;
        iax_session* session;

        void answer();

        void hangup(int reason);

        void refuse();

        void transfer(const std::string& to);

        bool attendedTransfer(const std::string& to);

        void onhold();

        void offhold();

        //TODO: implement mute for IAX
        void muteMedia(const std::string& mediaType, const bool& isMuted) {}

        void peerHungup();

        void carryingDTMFdigits(char code);

#if HAVE_INSTANT_MESSAGING
        void sendTextMessage(const std::string& message,
                             const std::string& from);
#endif

        void putAudioData(AudioBuffer& buf);

    private:
        NON_COPYABLE(IAXCall);

        // Incoming audio ring buffer
        std::shared_ptr<RingBuffer> ringbuffer_{};
};

} // namespace ring

#endif
