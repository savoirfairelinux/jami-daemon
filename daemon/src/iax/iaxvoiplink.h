/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
#ifndef IAXVOIPLINK_H
#define IAXVOIPLINK_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "audio/audiobuffer.h"
#include "audio/codecs/audiocodec.h" // for RAW_BUFFER_SIZE
#include "sfl_types.h"
#include "audio/resampler.h"

#include <iax/iax-client.h>

#include <mutex>
#include <memory>

class IAXAccount;
class IAXCall;
class AudioCodec;
class AudioLayer;

/**
 * @file iaxvoiplink.h
 * @brief VoIPLink contains a thread that listen to external events
 * and contains IAX Call related functions
 */

class IAXVoIPLink {
    public:
        IAXVoIPLink(IAXAccount& account);
        ~IAXVoIPLink();

        /**
         *	Listen to events sent by the call manager ( asterisk, etc .. )
         */
        void handleEvents();

        /**
         * Init the voip link
         */
        void init();

        /**
         * Terminate a voip link by clearing the call list
         */
        void terminate();

        /**
         * Cancel a call
         * @param id The ID of the call
         */
        void cancel(const std::string& /*id*/) {}

        /** Mutex for iax_ calls, since we're the only one dealing with the incorporated
         * iax_stuff inside this class. */
        static std::mutex mutexIAX;

    private:
        void handleAccept(iax_event* event, IAXCall& call);
        void handleReject(IAXCall& call);
        void handleRinging(IAXCall& call);
        void handleAnswerTransfer(iax_event* event, IAXCall& call);
        void handleBusy(IAXCall& call);
#if HAVE_INSTANT_MESSAGING
        void handleMessage(iax_event* event, IAXCall& call);
#endif
        void handleHangup(IAXCall& call);

        /*
         * Decode the message count IAX send.
         * Returns only the new messages number
         *
         * @param msgcount  The value sent by IAX in the REGACK message
         * @return int  The number of new messages waiting for the current registered user
         */
        int processIAXMsgCount(int msgcount);


        /**
         * Find a iaxcall by iax session number
         * @param session an iax_session valid pointer
         * @return iaxcall or 0 if not found
         */
        std::string iaxFindCallIDBySession(struct iax_session* session);

        /**
         * Handle IAX Event for a call
         * @param event An iax_event pointer
         * @param call  An IAXCall pointer
         */
        void iaxHandleCallEvent(iax_event* event, IAXCall& call);

        /**
         * Handle the VOICE events specifically
         * @param event The iax_event containing the IAX_EVENT_VOICE
         * @param call  The associated IAXCall
         */
        void iaxHandleVoiceEvent(iax_event* event, IAXCall& call);

        /**
         * Handle IAX Registration Reply event
         * @param event An iax_event pointer
         */
        void iaxHandleRegReply(iax_event* event);

        /**
         * Handle IAX pre-call setup-related events
         * @param event An iax_event pointer
         */
        void iaxHandlePrecallEvent(iax_event* event);

        /**
         * Work out the audio data from Microphone to IAX2 channel
         */
        void sendAudioFromMic();

        /** encoder/decoder/resampler buffers */
        sfl::AudioBuffer rawBuffer_{RAW_BUFFER_SIZE, sfl::AudioFormat::MONO()};
        sfl::AudioBuffer resampledData_{RAW_BUFFER_SIZE * 4, sfl::AudioFormat::MONO()};
        unsigned char encodedData_[RAW_BUFFER_SIZE] = {};

        sfl::Resampler resampler_{44100};

        /** Whether init() was called already or not
         * This should be used in init() and terminate(), to
         * indicate that init() was called, or reset by terminate().
         */
        bool initDone_{false};

        IAXAccount& account_;
};

#endif
