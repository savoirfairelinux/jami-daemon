/*
 *  Copyright (C) 2006-2007-2008 Savoir-Faire Linux inc.
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
 */
#ifndef IAXVOIPLINK_H
#define IAXVOIPLINK_H

#include "voiplink.h"
#include <iax2/iax-client.h>
#include "global.h"

#include "audio/codecDescriptor.h"
#include "samplerateconverter.h"

class EventThread;
class IAXCall;

class AudioCodec;
class AudioLayer;

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
    IAXVoIPLink(const AccountID& accountID);

    /**
     * Destructor
     */
    ~IAXVoIPLink();

    /**
     *	Listen to events sent by the call manager ( asterisk, etc .. )
     */
    void getEvent(void);
    
    /**
     * Init the voip link
     * @return true if successful
     *	      false otherwise
     */
    bool init (void);
    
    /**
     * Terminate a voip link by clearing the call list
     */
    void terminate (void);

    /**
     * Send out registration
     * @return bool The new registration state (are we registered ?)
     */
    int sendRegister (AccountID id);

    /**
     * Destroy registration session
     * @todo Send an IAX_COMMAND_REGREL to force unregistration upstream.
     *       Urgency: low
     * @return bool true if we're registered upstream
     *		  false otherwise
     */
    int sendUnregister (AccountID id);

    /**
     * Create a new outgoing call
     * @param id  The ID of the call
     * @param toUrl The address to call
     * @return Call*  A pointer on the call
     */
    Call* newOutgoingCall(const CallID& id, const std::string& toUrl);
    
    /**
     * Answer a call 
     * @param id The ID of the call
     * @return bool true on success
     *		  false otherwise
     */
    bool answer(const CallID& id);

    /**
     * Hangup a call 
     * @param id The ID of the call
     * @return bool true on success
     *		  false otherwise
     */
    bool hangup(const CallID& id);

    /**
     * Cancel a call 
     * @param id The ID of the call
     * @return bool true on success
     *		  false otherwise
     */
    bool cancel(const CallID& id UNUSED ) { return false; }
    
    /**
     * Put a call on hold 
     * @param id The ID of the call
     * @return bool true on success
     *		  false otherwise
     */
    bool onhold(const CallID& id);
    
    /**
     * Put a call off hold
     * @param id The ID of the call
     * @return bool true on success
     *		  false otherwise
     */
    bool offhold(const CallID& id);
    
    /**
     * Transfer a call 
     * @param id The ID of the call
     * @param to The recipient of the transfer
     * @return bool true on success
     *		  false otherwise
     */
    bool transfer(const CallID& id, const std::string& to);
    
    /**
     * Refuse a call 
     * @param id The ID of the call
     * @return bool true on success
     *		  false otherwise
     */
    bool refuse (const CallID& id);
    
    /**
     * Send DTMF  
     * @param id The ID of the call
     * @param code  The code of the DTMF
     * @return bool true on success
     *		  false otherwise
     */
    bool carryingDTMFdigits(const CallID& id, char code);
    
    bool sendMessage(const std::string& to UNUSED, const std::string& body UNUSED) { return false; }
    
    bool isContactPresenceSupported() { return false; }

  public: // iaxvoiplink only
    /**
     * @param host Set the host name 
     */
    void setHost(const std::string& host) { _host = host; }

    /**
     * @param user Set the user name 
     */
    void setUser(const std::string& user) { _user = user; }
    
    /**
     * @param pass Set the password
     */
    void setPass(const std::string& pass) { _pass = pass; }

    void updateAudiolayer( void ); 

    void setStunServer( const std::string &server ) {};

  private:

    /* 
     * Decode the message count IAX send.
     * Returns only the new messages number
     * 
     * @param msgcount  The value sent by IAX in the REGACK message
     * @return int  The number of new messages waiting for the current registered user
     */
       int processIAXMsgCount( int msgcount );


    /**
     * Get IAX Call from an id
     * @param id CallId
     * @return IAXCall pointer or 0
     */
    IAXCall* getIAXCall(const CallID& id);

    /**
     * Delete every call 
     */
    void terminateIAXCall();

    /**
     * Find a iaxcall by iax session number
     * @param session an iax_session valid pointer
     * @return iaxcall or 0 if not found
     */
    IAXCall* iaxFindCallBySession(struct iax_session* session);

    /**
     * Handle IAX Event for a call
     * @param event An iax_event pointer
     * @param call  An IAXCall pointer 
     */
    void iaxHandleCallEvent(iax_event* event, IAXCall* call);

    /**
     * Handle the VOICE events specifically
     * @param event The iax_event containing the IAX_EVENT_VOICE
     * @param call  The associated IAXCall
     */
    void iaxHandleVoiceEvent(iax_event* event, IAXCall* call);

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
    void sendAudioFromMic(void);

    /**
     * Send an outgoing call invite to iax
     * @param call An IAXCall pointer
     */
    bool iaxOutgoingInvite(IAXCall* call);


    /**
     * Convert CodecMap to IAX format using IAX constants
     * @return `format` ready to go into iax_* calls
     */
    int iaxCodecMapToFormat(IAXCall* call);

    /** Threading object */
    EventThread* _evThread;

    /** registration session : 0 if not register */
    struct iax_session* _regSession;

    /** IAX Host */
    std::string _host;

    /** IAX User */
    std::string _user;

    /** IAX Password */
    std::string _pass;

    /** Timestamp of when we should refresh the registration up with
     * the registrar.  Values can be: EPOCH timestamp, 0 if we want no registration, 1
     * to force a registration. */
    int _nextRefreshStamp;

    /** Mutex for iax_ calls, since we're the only one dealing with the incorporated
     * iax_stuff inside this class. */
    ost::Mutex _mutexIAX;

    /** Connection to audio card/device */
    AudioLayer* audiolayer;

    /** Mic-data related buffers */
    SFLDataFormat* micData;
    SFLDataFormat* micDataConverted;
    unsigned char* micDataEncoded;

    /** Speaker-data related buffers */
    SFLDataFormat* spkrDataDecoded;
    SFLDataFormat* spkrDataConverted;

    /** Sample rate converter object */
    SamplerateConverter* converter;

};

#endif
