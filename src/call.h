/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#include <string>
#include <cc++/thread.h> // for mutex

typedef std::string CallID;

/**
 * A call is the base classes for protocol-based calls
 * @author Yan Morin <yan.morin@gmail.com>
 */
class Call{
public:
    enum CallType {Incoming, Outgoing};
    enum ConnectionState {Disconnected, Trying, Progressing, Ringing, Connected };
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
    const std::string& getPeerNumber() {  return _peerNumber; }

    /** 
     * Set the peer name (caller in ingoing)
     * not protected by mutex (when created)
     * @param number peer number
     */
    void setPeerName(const std::string& name) {  _peerName = name; }
    const std::string& getPeerName() {  return _peerName; }

    /**
     * Tell if the call is incoming
     */
    bool isIncoming() { return (_type == Incoming) ? true : false; }

    /** 
     * Set the connection state of the call (protected by mutex)
     */
    void setConnectionState(ConnectionState state);
    /** 
     * get the connection state of the call (protected by mutex)
     */
    ConnectionState getConnectionState();

    /**
     * Set the state of the call (protected by mutex)
     */
    void setState(CallState state);
    /** 
     * get the call state of the call (protected by mutex)
     */
    CallState getState();

protected:
    /** Protect every attribute that can be changed by two threads */
    ost::Mutex _callMutex;

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
};

#endif
