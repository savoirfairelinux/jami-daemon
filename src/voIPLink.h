/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
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

#ifndef __VOIP_LINK_H__
#define __VOIP_LINK_H__

#include <string>

enum VoIPLinkType {
	Sip = 0,
	Iax
};

class AudioCodec;
class Call;

class VoIPLink {
public:
	VoIPLink (short id);
	virtual ~VoIPLink (void);

	// Pure virtual functions
	virtual int getEvent (void) = 0;
	virtual int init (void) = 0;
	virtual bool checkNetwork (void) = 0;
	virtual void terminate (void) = 0;
	virtual void newOutgoingCall (short callid) = 0;
	virtual void newIncomingCall (short callid) = 0;
	virtual int outgoingInvite (short id, const std::string& to_url) = 0;
	virtual int answer (short id) = 0;
	virtual int hangup (short id) = 0;
	virtual int cancel (short id) = 0;
	virtual int onhold (short id) = 0;
	virtual int offhold (short id) = 0;
	virtual int transfer (short id, const std::string& to) = 0;
	virtual int refuse (short id) = 0;
	virtual int setRegister (void) = 0;
	virtual int setUnregister (void) = 0;
	virtual void carryingDTMFdigits(short id, char code) = 0;
	virtual AudioCodec* getAudioCodec (short callid) = 0;
	 
	void setId (short id);
	short getId (void);
	void setType (VoIPLinkType type);
	VoIPLinkType getType (void);
	void setFullName (const std::string& fullname);
	std::string getFullName (void);
	void setHostName (const std::string& hostname);
	std::string getHostName (void);
	void setLocalIpAddress (const std::string& ipAdress);
	std::string getLocalIpAddress (void);

	
protected:

private:
	void initConstructor(void);
	
	short _id;
	VoIPLinkType _type;
	std::string _fullname;
	std::string _hostname;
	std::string _localIpAddress;
};

#endif // __VOIP_LINK_H__
