/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <ccrtp/rtp.h>

#include <qapplication.h>
#include <qhostaddress.h>
#include <qwidget.h>

#include "../stund/udp.h"
#include "../stund/stun.h"

#include "audiodriversoss.h"

#ifdef ALSA
#include "audiodriversalsa.h"
#endif

#include "configuration.h"
#include "configurationtree.h"
#include "global.h"
#include "manager.h"
#include "audiortp.h"
#include "sip.h"
#include "qtGUImainwindow.h"
#include "error.h"

#include <string>
using namespace std;

Manager::Manager (QString *Dc = NULL) {
	DirectCall = Dc;
	for (int i = 0; i < NUMBER_OF_LINES; i++) {
		phLines[i] = new PhoneLine ();
	}
	
	exist = createSettingsPath();
	
	selectAudioDriver();
	phonegui = new QtGUIMainWindow (0, 0 ,  
					Qt::WDestructiveClose | 
					Qt::WStyle_Customize |
					Qt::WStyle_NoBorder,this);

	sip = new SIP(this);
	tone = new ToneGenerator(this);
	audioRTP = new AudioRtp(this);
	error = new Error(this);
	
	sip_init();

	// Init variables
	b_ringing = false;
	b_ringtone = false;
	b_congestion = false;
	mute = false;

	if (DirectCall) { 
		qWarning ("Direct call.....");
		gui()->lcd->textBuffer = DirectCall ;
		gui()->dial();
	}

	if (!exist){
		// If config file $HOME/.PROGNAME/PROGNAMErc doesn't exist,
  		// show configuration panel
		gui()->configuration();
	} 

	initVolume ();	
}

Manager::~Manager (void) {
	delete phonegui;
	delete sip;
	delete audioRTP;
	delete audiodriver;
#ifdef ALSA
	delete audiodriverReadAlsa;
#endif
	delete tone;
	delete[] phLines;
}

/**
 * Create .PROGNAME directory in home user and create configuration tree from
 * the settings file if this file exists.
 *
 * @return	true if config-file exists or false if not.
 */
bool
Manager::createSettingsPath (void) {
	// 
	bool exist = true;
	char * buffer;
	// Get variable $HOME
  	buffer = getenv ("HOME");                                        
	path = string(buffer);
  	path = path + "/." + PROGNAME;
             
  	if (mkdir (path.data(), 0755) != 0) {
		// If directory	creation failed
    	if (errno != EEXIST) {
        	printf ("Cannot create directory: %s\n", strerror(errno));
      	} 
  	}
  
	// Load user's config
	path = path + "/" + PROGNAME + "rc";
	if (Config::tree()->populateFromFile(path.data()) == 0){
		exist = false;
	}
	return exist;
}
 
/**
 * Call audio driver constructor according to the selected driver in setup
 */
void 
Manager::selectAudioDriver (void) {
	if (Config::getb("Audio", "Drivers.driverOSS")) {
		useAlsa = false; 
		this->audiodriver = new AudioDriversOSS (AudioDrivers::ReadWrite, error);
	}
	if (Config::getb("Audio", "Drivers.driverALSA")) {
#ifdef ALSA
		useAlsa = true;
		this->audiodriver = new AudioDriversALSA (AudioDrivers::WriteOnly, error);
		this->audiodriverReadAlsa = new AudioDriversALSA (AudioDrivers::ReadOnly, error);
#endif
	}
}

void
Manager::initVolume (void) {
	spkr_volume = gui()->spkrVolVector->Y() - gui()->vol_spkr_y;
	mic_volume = gui()->micVolVector->Y() - gui()->vol_mic_y;
}

/**
 * Init the SIP stack
 */
void
Manager::sip_init (void) {
	if ( sip->initSIP () != -1) {
		 sip->initRtpmapCodec ();
	}
	
	if (Config::getb("Signalisations", "SIP.autoregister")) {
		// Register to the known proxies if available
		if (Config::gets("Signalisations", "SIP.password").length() > 0) {
			sip->setRegister ();
		} else {
			if (exist)
				error->errorName(PASSWD_FIELD_EMPTY, NULL);		
		}
	} 
}

void
Manager::quitLibrary (void) {
	sip->quitSIP();
}

int
Manager::outgoingNewCall (void) {
	return sip->outgoingInvite();
}

bool
Manager::ringing (void) {
	return this->b_ringing;
}

// When IP-phone user receives a call 
void
Manager::ring (bool var) {
	if (sip->getNumberPendingCalls() != 1 and tonezone == true 
			and var == false) {
	// If more than one line is ringing
		for (int i = 0; i < NUMBER_OF_LINES; i++) {
			if (i != getCurrentLineNumber() and phLines[i]->getbRinging()) {
				tonezone = false;
				tone->playRingtone ((gui()->getRingFile()).ascii());
			}
		}
	}

	if (this->b_ringing != var) {
		this->b_ringing = var;
	}

	tonezone = ringing();

	if (sip->getNumberPendingCalls() == 1) { 
	// If just one line is ringing
		tone->playRingtone ((gui()->getRingFile()).ascii());
	} 
}

// When IP-phone user makes call
void
Manager::ringTone (bool var) {
	if (this->b_ringtone != var) {
		this->b_ringtone = var;
	}
	tonezone = var;
	tone->toneHandle(ZT_TONE_RINGTONE);
}

void
Manager::congestion (bool var) {
	if (error->getError() == 0) {
		if (this->b_congestion != var) {
			this->b_congestion = var;
		}
		tonezone = var;
		tone->toneHandle(ZT_TONE_CONGESTION);
	} else {
		error->errorName(DEVICE_NOT_OPEN, NULL);
	}
}

void
Manager::notificationIncomingCall (void) {
	short *buffer = new short[SAMPLING_RATE];
	
	tone->generateSin(440, 0, AMPLITUDE, SAMPLING_RATE, buffer);
		
	audiodriver->audio_buf.resize(SAMPLING_RATE/2);
	audiodriver->audio_buf.setData(buffer, getSpkrVolume());
	delete[] buffer;
}
#if 0
bool
Manager::getCallInProgress (void) {
	return gui()->callinprogress;
}

void
Manager::setCallInProgress (bool inprogress) {
	gui()->callinprogress = inprogress;
}
#endif

bool
Manager::transferedCall(void) {
	return gui()->transfer;
}

/**
 * Handle IP_Phone user's actions
 *
 * @param	lineNumber: line where occurs the action
 * @param 	action:		type of action
 */
void
Manager::actionHandle (int lineNumber, int action) {
	switch (action) {
	case REFUSE_CALL:
		sip->manageActions (lineNumber, REFUSE_CALL);
		this->ring(false);
		phLines[lineNumber]->setbRinging(false);
		gui()->lcd->setStatus(REFUSED_CALL_STATUS);
		break;
		
	case ANSWER_CALL:
		// TODO
		// Stopper l'etat "ringing" du main (audio, signalisation, gui)
		this->ring(false);
		sip->manageActions (lineNumber, ANSWER_CALL);		
		phLines[lineNumber]->setbRinging(false);
		gui()->lcd->setStatus(CONNECTED_STATUS);	
		gui()->startCallTimer(lineNumber);
		break;

	case CLOSE_CALL:
		// If the call is active TODO: or is ringing for the callee
		if (sip->call[lineNumber] != NULL) {
			// stop call timer			
			gui()->stopCallTimer(lineNumber);
			sip->manageActions (lineNumber, CLOSE_CALL);
			sip->notUsedLine = -1;	
		}		
		break;

	case ONHOLD_CALL:
		if (sip->call[lineNumber] != NULL) {
			sip->manageActions (lineNumber, ONHOLD_CALL);
		}
		break;
		
	case OFFHOLD_CALL:
		if (sip->call[lineNumber] != NULL) {
			sip->manageActions (lineNumber, OFFHOLD_CALL);
		}
		break;
		
	case TRANSFER_CALL:
		sip->manageActions (lineNumber, TRANSFER_CALL);
		break;
		
	case CANCEL_CALL:
		sip->manageActions (lineNumber, CANCEL_CALL);
		break;

	default:
		break;
	}
}

int
Manager::newCallLineNumber (void) {
	return sip->notUsedLine;
}

bool
Manager::isRingingLine (int line) {
	if (line == this->newCallLineNumber()) {
		return true;
	}
	return false;
}

bool
Manager::isNotUsedLine (int line) {
	for (int k = 0; k < NUMBER_OF_LINES; k++) {
		if (line == k) {
			if (sip->call[line] == NULL) {
				return true;
			}
		}
	}
	return false;
}

bool
Manager::isUsedLine (int line) {
	for (int k = 0; k < NUMBER_OF_LINES; k++) {
		if (line == k) {
			if (sip->call[line] != NULL) {
				return true;
			}
		}
	}
	return false;
}


int
Manager::findLineNumberNotUsed (void) {
	return sip->findLineNumberNotUsed();
}

int
Manager::getNumberPendingCalls (void) {
	return sip->getNumberPendingCalls();
}
/**
 * Handle the remote callee's events
 *
 * @param	code:		event code
 * @param	reason:		event reason
 * @param	remotetype:	event type
 */
void
Manager::handleRemoteEvent (int code, char * reason, int remotetype, int line) {
	QString qinfo;
	
	switch (remotetype) {
		// Registration success
		case EXOSIP_REGISTRATION_SUCCESS: 
			gui()->lcd->setStatus(LOGGED_IN_STATUS);				
			break;

		// Registration failure
		case EXOSIP_REGISTRATION_FAILURE:
			gui()->lcd->setStatus(REG_FAIL_STATUS);				
			break;
			
		// Remote callee answered
		case EXOSIP_CALL_ANSWERED:
			if (!gui()->transfer)
				gui()->lcd->setStatus(CONNECTED_STATUS);
			// Start call timer
			gui()->startCallTimer(getCurrentLineNumber());
			phLines[getCurrentLineNumber()]->setbDial(false);
			break;

		// Remote callee hangup
		case EXOSIP_CALL_CLOSED:
			if (sip->getNumberPendingCalls() == 0 
					or !phLines[line]->isOnHold()) {
			// 	Show HUNGUP_STATUS if there's not pending call 
			//	or line is not onhold
				gui()->lcd->clear(QString(HUNGUP_STATUS));
			}

			if (phLines[line]->getbInProgress()) {
				this->ring(false);
				phLines[line]->setbRinging(false);
				gui()->setFreeStateLine(newCallLineNumber());
				//setCallInProgress(false);
				phLines[line]->setbInProgress(false);
			} else {
				// Stop call timer
				gui()->stopCallTimer(line);
				// set state free pixmap line
				gui()->setFreeStateLine(line);
				// set free line
				gui()->setCurrentLineNumber(-1);
			}
			phLines[line]->setbDial(false);
			
			sip->notUsedLine = -1;
			break;

		// Remote call ringing
		case EXOSIP_CALL_RINGING:
			gui()->lcd->setStatus(RINGING_STATUS);
			phLines[getCurrentLineNumber()]->setbDial(false);
			break;
			
		default:
			break;
	}

	// If dialog is established
	if (code == 101) {
		gui()->lcd->setStatus(RINGING_STATUS);		
	// if error code
	} else { 	
		if (code == AUTH_REQUIRED) {
			gui()->lcd->setStatus(TRYING_STATUS);
		} else if (code > 399 and code != AUTH_REQUIRED and 
					code != REQ_TERMINATED) {
			qinfo = QString::number(code, 10) + " " + 
				QString(reason);
			gui()->lcd->setStatus(qinfo);
		} else if (0 < code < 400) {
			qinfo = QString(reason);
		}
	}
		
	// Store the status of current line
	if(getCurrentLineNumber() != -1) {
		phLines[getCurrentLineNumber()]->status = getStatusRender();	
		//qDebug("line[%d] status: %s", getCurrentLineNumber(),
		//		(phLines[getCurrentLineNumber()]->status).ascii());
	}
}

bool
Manager::tryingState (int line) {
	if (phLines[line]->status == TRYING_STATUS) {
		return true;
	} else {
		return false;
	}
}

void
Manager::startDialTone (void) {
	gui()->dialTone(true);
}

int
Manager::startSound (SipCall *ca) {
	return audioRTP->createNewSession(ca);
}

void
Manager::closeSound (SipCall *ca) {
	audioRTP->closeRtpSession (ca);
}

QString
Manager::bufferTextRender (void) {
	return QString(*gui()->lcd->textBuffer);
}

QString
Manager::getStatusRender (void) {
	return (gui()->lcd->getStatus());
}

void
Manager::getInfoStun (StunAddress4& stunSvrAddr) {
	StunAddress4 mappedAddr;
	
	int fd3, fd4; 
    bool ok = stunOpenSocketPair(stunSvrAddr,
                                      &mappedAddr,
                                      &fd3,
                                      &fd4);
	if (ok) {
    	closesocket(fd3);
        closesocket(fd4);
        qDebug("Got port pair at %d", mappedAddr.port);
		firewallPort = mappedAddr.port;
		QHostAddress ha(mappedAddr.addr);
		firewallAddr = ha.toString();
		qDebug("address firewall = %s",firewallAddr.ascii());
	} else {
    	qDebug("Opened a stun socket pair FAILED");
    }
}

int
Manager::getFirewallPort (void) {
	return firewallPort;
}

void
Manager::setFirewallPort (int port) {
	firewallPort = port;
}

QString
Manager::getFirewallAddress (void) {
	return firewallAddr;
}

/**
 * Returns true if the current line replaces another one
 */
bool
Manager::otherLine (void) {
	if (gui()->busyNum != -1) {
		if (phLines[getCurrentLineNumber()]->status != TRYING_STATUS and
				gui()->busyNum != getCurrentLineNumber()) {
			return true;
		}
	}
	return false;
}

int
Manager::getCurrentLineNumber (void) {
	return gui()->currentLineNumber;
}

bool
Manager::isChosenLine (void) {
	return gui()->choose;
}

int
Manager::chosenLine (void) {
	return gui()->chosenLine;
}

void
Manager::setChoose (bool b, bool b2) {
	gui()->choose = b;
	gui()->noChoose = b2;
}

bool
Manager::useStun () {
	if (Config::geti("Signalisations", "STUN.useStun") == YES) {
		return true;
	} else {
		return false;
	}
}

/**
 * Handle choice of the DTMF-send-way
 *
 * @param	line: number of the line.
 * @param	digit: pressed key.
 */
void
Manager::dtmf (int line, char digit) {
	int sendType = Config::geti ("Signalisations", "DTMF.sendDTMFas");
	
	switch (sendType) {
		// SIP INFO
		case 0:
			if (sip->call[line] != NULL) {
				sip->carryingDTMFdigits(line, digit);
			}
			break;

		// Audio way
		case 1:
			break;
			
		// rfc 2833
		case 2:
			break;
			
		default:
			break;
	}
}

void
Manager::errorDisplay (char *error) {
	gui()->lcd->clearBuffer();
	gui()->lcd->appendText(error);
}

void
Manager::nameDisplay (char *name) {
	gui()->lcd->clearBuffer();
	gui()->lcd->appendText(name);
}

void
Manager::spkrSoundVolume (int val) {
	spkr_volume = val;
}

void
Manager::micSoundVolume (int val) {
	mic_volume = val;
}
