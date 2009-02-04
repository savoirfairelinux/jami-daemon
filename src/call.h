/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
#ifndef CALL_H
#define CALL_H

#include <cc++/thread.h> // for mutex
#include <sstream>

#include "audio/codecDescriptor.h"
#include "plug-in/audiorecorder/audiorecord.h"

/* 
 * @file call.h 
 * @brief A call is the base class for protocol-based calls
 */

typedef std::string CallID;

class Call{
  public:
    /**
     * This determines if the call originated from the local user (Outgoing)
     * or from some remote peer (Incoming).
     */
    enum CallType {Incoming, Outgoing};

    /**
     * Tell where we're at with the call. The call gets Connected when we know
     * from the other end what happened with out call. A call can be 'Connected'
     * even if the call state is Busy, Refused, or Error.
     *
     * Audio should be transmitted when ConnectionState = Connected AND
     * CallState = Active.
     */
    enum ConnectionState {Disconnected, Trying, Progressing, Ringing, Connected};

    /**
     * The Call State.
     */
    enum CallState {Inactive, Active, Hold, Busy, Refused, Error};

    /**
     * Constructor of a call
     * @param id Unique identifier of the call
     * @param type set definitely this call as incoming/outgoing
     */
    Call(const CallID& id, Call::CallType type);
    virtual ~Call();

    /** 
     * Return a reference on the call id
     * @return call id
     */
    CallID& getCallId() {return _id; }

    /** 
     * Set the peer number (destination on outgoing)
     * not protected by mutex (when created)
     * @param number peer number
     */
    void setPeerNumber(const std::string& number) {  _peerNumber = number; }

    /** 
     * Get the peer number (destination on outgoing)
     * not protected by mutex (when created)
     * @return std::string The peer number
     */
    const std::string& getPeerNumber() {  return _peerNumber; }

    /** 
     * Set the peer name (caller in ingoing)
     * not protected by mutex (when created)
     * @param name The peer name
     */
    void setPeerName(const std::string& name) {  _peerName = name; }

    /** 
     * Get the peer name (caller in ingoing)
     * not protected by mutex (when created)
     * @return std::string The peer name
     */
    const std::string& getPeerName() {  return _peerName; }

    /**
     * Tell if the call is incoming
     * @return true if yes
     *	      false otherwise
     */
    bool isIncoming() { return (_type == Incoming) ? true : false; }

    /** 
     * Set the connection state of the call (protected by mutex)
     * @param state The connection state
     */
    void setConnectionState(ConnectionState state);
    
    /** 
     * Get the connection state of the call (protected by mutex)
     * @return ConnectionState The connection state
     */
    ConnectionState getConnectionState();

    /**
     * Set the state of the call (protected by mutex)
     * @param state The call state
     */
    void setState(CallState state);

    /** 
     * Get the call state of the call (protected by mutex)
     * @return CallState  The call state
     */
    CallState getState();

    /**
     * Set the audio start boolean (protected by mutex)
     * @param start true if we start the audio
     *		    false otherwise
     */
    void setAudioStart(bool start);

    /**
     * Tell if the audio is started (protected by mutex)
     * @return true if it's already started
     *	      false otherwise
     */
    bool isAudioStarted();

    // AUDIO
    /** 
     * Set internal codec Map: initialization only, not protected 
     * @param map The codec map
     */
    void setCodecMap(const CodecDescriptor& map) { _codecMap = map; } 

    /** 
     * Get internal codec Map: initialization only, not protected 
     * @return CodecDescriptor	The codec map
     */
    CodecDescriptor& getCodecMap();

    /** 
     * Set my IP [not protected] 
     * @param ip  The local IP address
     */
    void setLocalIp(const std::string& ip)     { _localIPAddress = ip; }

    /** 
     * Set local audio port, as seen by me [not protected]
     * @param port  The local audio port
     */
    void setLocalAudioPort(unsigned int port)  { _localAudioPort = port;}

    /** 
     * Set the audio port that remote will see.
     * @param port  The external audio port
     */
    void setLocalExternAudioPort(unsigned int port) { _localExternalAudioPort = port; }

    /** 
     * Return the audio port seen by the remote side. 
     * @return unsigned int The external audio port
    */
    unsigned int getLocalExternAudioPort() { return _localExternalAudioPort; }

    /** 
     * Return my IP [mutex protected] 
     * @return std::string The local IP
     */
    const std::string& getLocalIp();

    /** 
     * Return port used locally (for my machine) [mutex protected] 
     * @return unsigned int  The local audio port
     */
    unsigned int getLocalAudioPort();

    /** 
     * Return audio port at destination [mutex protected] 
     * @return unsigned int The remote audio port
     */
    unsigned int getRemoteAudioPort();

    /** 
     * Return IP of destination [mutex protected]
     * @return const std:string	The remote IP address
     */
    const std::string& getRemoteIp();

    /** 
     * Return audio codec [mutex protected]
     * @return AudioCodecType The payload of the codec
     */
    AudioCodecType getAudioCodec();

    /**
     * @return Return the file name for this call
     */
    std::string getFileName() {return _filename;}

    /**
     * A recorder for this call
     */
    AudioRecord recAudio;
  
    /**
     * SetRecording
     */
    void setRecording();

    /**
     * stopRecording, make sure the recording is stopped (whe transfering call)
     */
    void stopRecording();
    
    /**
     * Return Recording state
     */
    bool isRecording(); 

  protected:
    /** Protect every attribute that can be changed by two threads */
    ost::Mutex _callMutex;

    /** 
     * Set remote's IP addr. [not protected]
     * @param ip  The remote IP address
     */
    void setRemoteIP(const std::string& ip)    { _remoteIPAddress = ip; }

    /** 
     * Set remote's audio port. [not protected]
     * @param port  The remote audio port
     */
    void setRemoteAudioPort(unsigned int port) { _remoteAudioPort = port; }

    /** 
     * Set the audio codec used.  [not protected] 
     * @param audioCodec  The payload of the codec
     */
    void setAudioCodec(AudioCodecType audioCodec) { _audioCodec = audioCodec; }

    /** Codec Map */
    CodecDescriptor _codecMap;

    /** Codec pointer */
    AudioCodecType _audioCodec;

    bool _audioStarted;

    // Informations about call socket / audio

    /** My IP address */
    std::string  _localIPAddress;

    /** Local audio port, as seen by me. */
    unsigned int _localAudioPort;

    /** Port assigned to my machine by the NAT, as seen by remote peer (he connects there) */
    unsigned int _localExternalAudioPort;

    /** Remote's IP address */
    std::string  _remoteIPAddress;

    /** Remote's audio port */
    unsigned int _remoteAudioPort;


  private:  
  
    /** Unique ID of the call */
    CallID _id;

    /** Type of the call */
    CallType _type;
    /** Disconnected/Progressing/Trying/Ringing/Connected */
    ConnectionState _connectionState;
    /** Inactive/Active/Hold/Busy/Refused/Error */
    CallState _callState;

    /** Name of the peer */
    std::string _peerName;

    /** Number of the peer */
    std::string _peerNumber;

    /** File name for his call : time YY-MM-DD */
    std::string _filename;
};

#endif
