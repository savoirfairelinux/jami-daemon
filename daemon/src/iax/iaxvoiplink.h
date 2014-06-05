/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#include <mutex>
#include "account.h"
#include "voiplink.h"
#include "audio/audiobuffer.h"
#include "audio/codecs/audiocodec.h" // for RAW_BUFFER_SIZE
#include "sfl_types.h"
#include "noncopyable.h"
#include "audio/resampler.h"

#include <iax-client.h>

#include <memory>

class IAXCall;
class IAXAccount;

class AudioCodec;
class AudioLayer;

typedef std::map<std::string, std::shared_ptr<IAXCall> > IAXCallMap;

/**
 * @file iaxvoiplink.h
 * @brief VoIPLink contains a thread that listen to external events
 * and contains IAX Call related functions
 */

class IAXVoIPLink : public VoIPLink {
    public:

        IAXVoIPLink(const std::string& accountID);
        ~IAXVoIPLink();

        /**
         *	Listen to events sent by the call manager ( asterisk, etc .. )
         */
        virtual bool handleEvents();


        /* Returns a list of all callIDs */
        static std::vector<std::string> getCallIDs();

        virtual std::vector<std::shared_ptr<Call> > getCalls(const std::string &account_id) const;

        /**
         * Return the internal account map for all VOIP links
         */
        static AccountMap &getAccounts() { return iaxAccountMap_; }

        /**
         * Empty the account map for all VOIP links
         */
        static void unloadAccountMap();

        /**
         * Init the voip link
         */
        virtual void init();

        /**
         * Terminate a voip link by clearing the call list
         */
        void terminate();

        /**
         * Send out registration
         */
        virtual void sendRegister(Account& a);

        /**
         * Destroy registration session
         * @todo Send an IAX_COMMAND_REGREL to force unregistration upstream.
         *       Urgency: low
         */
        virtual void sendUnregister(Account& a, std::function<void(bool)> cb = std::function<void(bool)>());

        /**
         * Create a new outgoing call
         * @param id  The ID of the call
         * @param toUrl The address to call
         * @return Call*  A pointer on the call
         */
        virtual std::shared_ptr<Call> newOutgoingCall(const std::string& id, const std::string& toUrl, const std::string &account_id);

        /**
         * Answer a call
         * @param c The call
         */
        virtual void answer(Call *c);

        /**
         * Hangup a call
         * @param id The ID of the call
         */
        virtual void hangup(const std::string& id, int reason);

        /**
         * Peer Hungup a call
         * @param id The ID of the call
         */
        virtual void peerHungup(const std::string& id);

        /**
         * Cancel a call
         * @param id The ID of the call
         */
        virtual void cancel(const std::string& /*id*/) {}

        /**
         * Put a call on hold
         * @param id The ID of the call
         * @return bool true on success
         *		  false otherwise
         */
        virtual void onhold(const std::string& id);

        /**
         * Put a call off hold
         * @param id The ID of the call
         * @return bool true on success
         *		  false otherwise
         */
        virtual void offhold(const std::string& id);

        /**
         * Transfer a call
         * @param id The ID of the call
         * @param to The recipient of the transfer
         */
        virtual void transfer(const std::string& id, const std::string& to);

        /**
         * Perform attended transfer
         * @param Transfered call ID
         * @param Target call ID
         * @return true on success
         */
        virtual bool attendedTransfer(const std::string& transferID, const std::string& targetID);

        /**
         * Refuse a call
         * @param id The ID of the call
         */
        virtual void refuse(const std::string& id);

        /**
         * Send DTMF
         * @param id The ID of the call
         * @param code  The code of the DTMF
         */
        virtual void carryingDTMFdigits(const std::string& id, char code);


#if HAVE_INSTANT_MESSAGING
        virtual void sendTextMessage(const std::string& callID, const std::string& message, const std::string& from);
#endif
        static void clearIaxCallMap();
        static void addIaxCall(std::shared_ptr<IAXCall>& call);
        // must be called while holding iaxCallMapMutex
        static std::shared_ptr<IAXCall> getIaxCall(const std::string& id);
        static void removeIaxCall(const std::string &id);

    private:
        NON_COPYABLE(IAXVoIPLink);

        void handleAccept(iax_event* event, const std::string &id);
        void handleReject(const std::string &id);
        void handleRinging(const std::string &id);
        void handleAnswerTransfer(iax_event* event, const std::string &id);
        void handleBusy(const std::string &id);
        void handleMessage(iax_event* event, const std::string &id);

        /**
         * Contains a list of all IAX account
         */
        static AccountMap iaxAccountMap_;

        static std::mutex iaxCallMapMutex_;
        static IAXCallMap iaxCallMap_;

        /*
         * Decode the message count IAX send.
         * Returns only the new messages number
         *
         * @param msgcount  The value sent by IAX in the REGACK message
         * @return int  The number of new messages waiting for the current registered user
         */
        int processIAXMsgCount(int msgcount);


        /**
         * Get IAX Call from an id
         * @param id CallId
         * @return IAXCall pointer or 0
         */
        std::shared_ptr<IAXCall> getIAXCall(const std::string& id);

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
        void iaxHandleCallEvent(iax_event* event, const std::string &id);

        /**
         * Handle the VOICE events specifically
         * @param event The iax_event containing the IAX_EVENT_VOICE
         * @param call  The associated IAXCall
         */
        void iaxHandleVoiceEvent(iax_event* event, const std::string &id);

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

        /**
         * Send an outgoing call invite to iax
         * @param call An IAXCall pointer
         */
        void iaxOutgoingInvite(IAXCall* call);

        /** registration session : 0 if not register */
        iax_session* regSession_;

        /** Timestamp of when we should refresh the registration up with
         * the registrar.  Values can be: EPOCH timestamp, 0 if we want no registration, 1
         * to force a registration. */
        int nextRefreshStamp_;

        /** Mutex for iax_ calls, since we're the only one dealing with the incorporated
         * iax_stuff inside this class. */
        std::mutex mutexIAX_;

        /** encoder/decoder/resampler buffers */
        AudioBuffer rawBuffer_;
        AudioBuffer resampledData_;
        unsigned char encodedData_[RAW_BUFFER_SIZE];

        Resampler resampler_;

        /** Whether init() was called already or not
         * This should be used in init() and terminate(), to
         * indicate that init() was called, or reset by terminate().
         */
        bool initDone_;

        const std::string accountID_;
};

#endif
