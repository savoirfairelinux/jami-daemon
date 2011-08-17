/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include "voiplink.h"
#include <iax-client.h>
#include "global.h"

#include "audio/codecs/audiocodecfactory.h"
#include "audio/samplerateconverter.h"
#include "hooks/urlhook.h"

#include "im/InstantMessaging.h"

class EventThread;
class IAXCall;

class AudioCodec;
class AudioLayer;

class IAXAccount;

/**
 * @file iaxvoiplink.h
 * @brief VoIPLink contains a thread that listen to external events
 * and contains IAX Call related functions
 */

class IAXVoIPLink : public VoIPLink
{
    public:

        /**
         * Constructor
         * @param accountID	The account containing the voip link
         */
        IAXVoIPLink (const std::string& accountID);

        /**
         * Destructor
         */
        ~IAXVoIPLink();

        /**
         *	Listen to events sent by the call manager ( asterisk, etc .. )
         */
        void getEvent (void);

        /**
         * Init the voip link
         * @return true if successful
         *	      false otherwise
         */
        virtual bool init (void);

        /**
         * Terminate a voip link by clearing the call list
         */
        virtual void terminate (void);

        /**
         * Send out registration
         * @return bool The new registration state (are we registered ?)
         */
        virtual void sendRegister (std::string id) throw(VoipLinkException);

        /**
         * Destroy registration session
         * @todo Send an IAX_COMMAND_REGREL to force unregistration upstream.
         *       Urgency: low
         * @return bool true if we're registered upstream
         *		  false otherwise
         */
        virtual void sendUnregister (std::string id) throw(VoipLinkException);

        /**
         * Create a new outgoing call
         * @param id  The ID of the call
         * @param toUrl The address to call
         * @return Call*  A pointer on the call
         */
        virtual Call* newOutgoingCall (const std::string& id, const std::string& toUrl) throw(VoipLinkException);

        /**
         * Answer a call
         * @param id The ID of the call
         * @return bool true on success
         *		  false otherwise
         */
        virtual bool answer (const std::string& id) throw (VoipLinkException);

        /**
         * Hangup a call
         * @param id The ID of the call
         * @return bool true on success
         *		  false otherwise
         */
        virtual bool hangup (const std::string& id) throw (VoipLinkException);

        /**
         * Peer Hungup a call
         * @param id The ID of the call
         * @return bool true on success
         *		  false otherwise
         */
        virtual bool peerHungup (const std::string& id) throw (VoipLinkException);

        /**
         * Cancel a call
         * @param id The ID of the call
         * @return bool true on success
         *		  false otherwise
         */
        virtual bool cancel (const std::string& id UNUSED) throw (VoipLinkException){
            return false;
        }

        /**
         * Put a call on hold
         * @param id The ID of the call
         * @return bool true on success
         *		  false otherwise
         */
        virtual bool onhold (const std::string& id) throw (VoipLinkException);

        /**
         * Put a call off hold
         * @param id The ID of the call
         * @return bool true on success
         *		  false otherwise
         */
        virtual bool offhold (const std::string& id) throw (VoipLinkException);

        /**
         * Transfer a call
         * @param id The ID of the call
         * @param to The recipient of the transfer
         * @return bool true on success
         *		  false otherwise
         */
        virtual bool transfer (const std::string& id, const std::string& to) throw (VoipLinkException);

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
         * @return bool true on success
         *		  false otherwise
         */
        virtual bool refuse (const std::string& id);

        /**
         * Send DTMF
         * @param id The ID of the call
         * @param code  The code of the DTMF
         * @return bool true on success
         *		  false otherwise
         */
        virtual bool carryingDTMFdigits (const std::string& id, char code);


        virtual bool sendTextMessage (sfl::InstantMessaging *module, const std::string& callID, const std::string& message, const std::string& from);

        bool isContactPresenceSupported() {
            return false;
        }

        /**
         * Return the codec protocol used for this call
         * @param id The call identifier
         */
        virtual std::string getCurrentCodecName(const std::string& id);
        virtual std::string getCurrentVideoCodecName(const std::string& id);


    public: // iaxvoiplink only

        void updateAudiolayer (void);

    private:

        IAXAccount* getAccountPtr (void);

        /*
         * Decode the message count IAX send.
         * Returns only the new messages number
         *
         * @param msgcount  The value sent by IAX in the REGACK message
         * @return int  The number of new messages waiting for the current registered user
         */
        int processIAXMsgCount (int msgcount);


        /**
         * Get IAX Call from an id
         * @param id CallId
         * @return IAXCall pointer or 0
         */
        IAXCall* getIAXCall (const std::string& id);

        /**
         * Delete every call
         */
        void terminateIAXCall();

        /**
         * Find a iaxcall by iax session number
         * @param session an iax_session valid pointer
         * @return iaxcall or 0 if not found
         */
        IAXCall* iaxFindCallBySession (struct iax_session* session);

        /**
         * Handle IAX Event for a call
         * @param event An iax_event pointer
         * @param call  An IAXCall pointer
         */
        void iaxHandleCallEvent (iax_event* event, IAXCall* call);

        /**
         * Handle the VOICE events specifically
         * @param event The iax_event containing the IAX_EVENT_VOICE
         * @param call  The associated IAXCall
         */
        void iaxHandleVoiceEvent (iax_event* event, IAXCall* call);

        /**
         * Handle IAX Registration Reply event
         * @param event An iax_event pointer
         */
        void iaxHandleRegReply (iax_event* event);

        /**
         * Handle IAX pre-call setup-related events
         * @param event An iax_event pointer
         */
        void iaxHandlePrecallEvent (iax_event* event);

        /**
         * Work out the audio data from Microphone to IAX2 channel
         */
        void sendAudioFromMic (void);

        /**
         * Send an outgoing call invite to iax
         * @param call An IAXCall pointer
         */
        bool iaxOutgoingInvite (IAXCall* call);

        /** Threading object */
        EventThread* _evThread;

        /** registration session : 0 if not register */
        struct iax_session* _regSession;

        /** Timestamp of when we should refresh the registration up with
         * the registrar.  Values can be: EPOCH timestamp, 0 if we want no registration, 1
         * to force a registration. */
        int _nextRefreshStamp;

        /** Mutex for iax_ calls, since we're the only one dealing with the incorporated
         * iax_stuff inside this class. */
        ost::Mutex _mutexIAX;

        /** Connection to audio card/device */
        AudioLayer* audiolayer;

        /** encoder/decoder/resampler buffers */
        SFLDataFormat decData[DEC_BUFFER_SIZE];
        SFLDataFormat resampledData[DEC_BUFFER_SIZE];
        unsigned char encodedData[DEC_BUFFER_SIZE];

        /** Sample rate converter object */
        SamplerateConverter* converter;

        int converterSamplingRate;

        /* URL hook */
        UrlHook *urlhook;

        const std::string _accountID;
};

#endif
