/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#ifndef __MANAGER_H__
#define __MANAGER_H__


#include "audiodrivers.h"
#include "phoneline.h"
#include "../stund/stun.h"

class AudioRtp;
class SIP;
class SipCall;
class ToneGenerator;
class QtGUIMainWindow;

class Manager {
public:
	Manager (QString *);
	~Manager (void);

	QtGUIMainWindow *phonegui;
	SIP 			*sip;
	PhoneLine		*phLines[NUMBER_OF_LINES];	
	AudioRtp		*audioRTP;
	AudioDrivers	*audiodriver;
	ToneGenerator	*tone;
	QString 	*DirectCall; // from argv[1]
	bool 			 mute;
	bool 			 tonezone;

	inline
	QtGUIMainWindow*gui			(void) { return this->phonegui; }
	bool	ringing 			(void);
	inline
	void 	ring    			(void) { this->ring(true); }
	void 	ring    			(bool);
	void 	quitLibrary 			(void);
	int		outgoingNewCall			(void);
	void 	actionHandle			(int, int);
	int 	findLineNumberNotUsedSIP	(void);
	void 	handleRemoteEvent		(int, char *, int);
	int	startSound			(SipCall *);
	void 	closeSound 			(SipCall *);	
	void	selectAudioDriver		(void);
	QString	bufferTextRender		(void);
	bool	isNotUsedLine			(int);
	bool	isRingingLine			(int);
	int	newCallLineNumber		(void);
	void	getInfoStun		       	(StunAddress4 &);
	int	getFirewallPort			(void);
	void	setFirewallPort 		(int);
	QString	getFirewallAddress		(void);
	bool	otherLine			(void);
	bool	isChosenLine			(void);
	int	chosenLine			(void);
	void	setChoose			(bool, bool);
	bool	useStun				(void);
	void	dtmf				(int, char);
	bool	getCallInProgress		(void);
	void	setCallInProgress		(bool);
	bool	transferedCall			(void);

	void	ringTone				(bool);

private:
	bool	b_ringing;
	bool	b_ringtone;
	int		firewallPort;
	QString	firewallAddr;

	void 	sip_rtp_init			(void);


};

#endif // __MANAGER_H__
