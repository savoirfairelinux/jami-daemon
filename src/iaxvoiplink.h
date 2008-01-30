/*
 *  Copyright (C) 2006-2007 Savoir-Faire Linux inc.
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
#include <iax/iax-client.h>
#include "global.h"
#include <samplerate.h>

/** @todo Remove this fstream/iostream stuff */
#include <fstream> // fstream + iostream for _fstream debugging...
#include <iostream>


class EventThread;
class IAXCall;

class AudioCodec;
class AudioLayer;

 
/**
 * VoIPLink contains a thread that listen to external events 
 * and contains IAX Call related functions
 * @author Yan Morin <yan.morin@gmail.com>
 */
class IAXVoIPLink : public VoIPLink
{
public:
    IAXVoIPLink(const AccountID& accountID);

    ~IAXVoIPLink();

  void getEvent(void);
  bool init (void);
  bool checkNetwork (void) { return false; }
  void terminate (void);

  /**
   * Send out registration
   *
   * @return The new registration state (are we registered ?)
   */
  bool sendRegister (void);

  /**
   * Destroy registration session
   *
   * @todo Send an IAX_COMMAND_REGREL to force unregistration upstream.
   *       Urgency: low
   *
   * @return bool If we're registered upstream
   */
  bool sendUnregister (void);

  Call* newOutgoingCall(const CallID& id, const std::string& toUrl);
  bool answer(const CallID& id);

  bool hangup(const CallID& id);
  bool cancel(const CallID& id) { return false; }
  bool onhold(const CallID& id);
  bool offhold(const CallID& id);
  bool transfer(const CallID& id, const std::string& to);
  bool refuse (const CallID& id);
  bool carryingDTMFdigits(const CallID& id, char code);
  bool sendMessage(const std::string& to, const std::string& body) { return false; }

public: // iaxvoiplink only
  void setHost(const std::string& host) { _host = host; }
  void setUser(const std::string& user) { _user = user; }
  void setPass(const std::string& pass) { _pass = pass; }

private:
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

 /**
  * Dynamically load an audio codec
  * @return audiocodec a pointer on an audiocodec object
  */ 
  AudioCodec* loadCodec(int payload);

 /**
  * Destroy and close the pointer on the codec
  * @param audiocodec the codec you want to unload
  */  
  void unloadCodec(AudioCodec* audiocodec);

  /** pointer on function **/
  void* handle_codec;

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

  /** IAX full name */
  std::string _fullName;

  /** Timestamp of when we should refresh the registration up with
   * the registrar.  Values can be: EPOCH timestamp, 0 if we want no registration, 1
   * to force a registration. */
  int _nextRefreshStamp;

  /** Mutex for iax_ calls, since we're the only one dealing with the incorporated
   * iax_stuff inside this class. */
  ost::Mutex _mutexIAX;

  /** Connection to audio card/device */
  AudioLayer* audiolayer;

  /** When we receive data, we decode it inside this buffer */
  int16* _receiveDataDecoded;
  /** When we send data, we encode it inside this buffer*/
  unsigned char* _sendDataEncoded;

  /** After that we send the data inside this buffer if there is a format conversion or rate conversion. */
  /* Also use for getting mic-ringbuffer data */
  SFLDataFormat* _dataAudioLayer;

  /** Buffer for 8000hz samples in conversion */
  float32* _floatBuffer8000;
  /** Buffer for 48000hz samples in conversion */ 
  float32* _floatBuffer48000;

  /** Buffer for 8000hz samples for mic conversion */
  int16* _intBuffer8000;

  /** libsamplerate converter for incoming voice */
  SRC_STATE*    _src_state_spkr;

  /** libsamplerate converter for outgoing voice */
  SRC_STATE*    _src_state_mic;

  /** libsamplerate error */
  int           _src_err;

  /** Debugging output file 
   * @todo Remove this */
  //std::ofstream _fstream;

};

#endif
