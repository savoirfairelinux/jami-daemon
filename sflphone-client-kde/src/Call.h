/***************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                              *
 *   Author : Jérémy Quentin                                               *
 *   jeremy.quentin@savoirfairelinux.com                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#ifndef CALL_H
#define CALL_H

#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtGui/QListWidgetItem>
#include <QtGui/QLabel>
#include <QtGui/QWidget>

#include "Contact.h"

/** @enum call_state_t 
  * This enum have all the states a call can take.
  */
typedef enum
{ 
   /** Ringing incoming call */
   CALL_STATE_INCOMING,
   /** Ringing outgoing call */
   CALL_STATE_RINGING,
   /** Call to which the user can speak and hear */
   CALL_STATE_CURRENT,
   /** Call which numbers are being added by the user */
   CALL_STATE_DIALING,
   /** Call is on hold */
   CALL_STATE_HOLD,
   /** Call has failed */
   CALL_STATE_FAILURE,
   /** Call is busy */
   CALL_STATE_BUSY,
   /** Call is being transfered.  During this state, the user can enter the new number. */
   CALL_STATE_TRANSFER,
   /** Call is on hold for transfer */
   CALL_STATE_TRANSF_HOLD,
   /** Call is over and should not be used */
   CALL_STATE_OVER,
   /** This state should never be reached */
   CALL_STATE_ERROR
} call_state;

/** @enum daemon_call_state_t 
  * This enum have all the states a call can take for the daemon.
  */
typedef enum
{ 
   /** Ringing outgoing or incoming call */
   DAEMON_CALL_STATE_RINGING,
   /** Call to which the user can speak and hear */
   DAEMON_CALL_STATE_CURRENT,
   /** Call is busy */
   DAEMON_CALL_STATE_BUSY,
   /** Call is on hold */
   DAEMON_CALL_STATE_HOLD,
   /** Call is over  */
   DAEMON_CALL_STATE_HUNG_UP,
   /** Call has failed */
   DAEMON_CALL_STATE_FAILURE
} daemon_call_state;

/** @enum call_action
  * This enum have all the actions you can make on a call.
  */
typedef enum
{ 
   /** Green button, accept or new call or place call or place transfer */
   CALL_ACTION_ACCEPT,
   /** Red button, refuse or hang up */
   CALL_ACTION_REFUSE,
   /** Blue button, put into or out of transfer mode where you can type transfer number */
   CALL_ACTION_TRANSFER,
   /** Blue-green button, hold or unhold the call */
   CALL_ACTION_HOLD,
   /** Record button, enable or disable recording */
   CALL_ACTION_RECORD,
} call_action;

/**
 * @enum history_state
 * This enum have all the state a call can take in the history
 */
typedef enum
{
  INCOMING,
  OUTGOING,
  MISSED,
  NONE
} history_state;


class Call;

typedef  void (Call::*function)();


/**
 *  This class represents a call either actual (in the call list
 *  displayed in main window), either past (in the call history).
 *  A call is represented by an automate, with a list of states
 *  (enum call_state) and 2 lists of transition signals
 *  (call_action when the user performs an action on the UI and 
 *  daemon_call_state when the daemon sends a stateChanged signal)
 *  When a transition signal is received, the automate calls a
 *  function then go to a new state according to the previous state
 *  of the call and the signal received.
 *  The functions to call and the new states to go to are placed in
 *  the maps actionPerformedStateMap, actionPerformedFunctionMap, 
 *  stateChangedStateMap and stateChangedFunctionMap.
 *  Those maps are used by actionPerformed and stateChanged functions
 *  to handle the behavior of the automate.
 *  When an actual call goes to the state OVER, it becomes part of
 *  the call history.
 *
 *  It may be better to handle call list and call history separately,
 *  and to use the class Item to handle their display, or a model/view
 *  way. For this it needs to handle the becoming of a call to a past call
 *  keeping the information gathered by the call and needed by the history
 *  call (history state, start time...).
**/
class Call
{
private:

	//Call attributes
	
	QString account;
	QString callId;
	QString peerPhoneNumber;
	QString peerName;
	history_state historyState;
	QDateTime * startTime;
	QDateTime * stopTime;
	
	QListWidgetItem * item;
	QWidget * itemWidget;
	QLabel * labelIcon;
	QLabel * labelPeerName;
	QLabel * labelCallNumber;
	QLabel * labelTransferPrefix;
	QLabel * labelTransferNumber;
	
	QListWidgetItem * historyItem;
	QWidget * historyItemWidget;
	QLabel * labelHistoryIcon;
	QLabel * labelHistoryPeerName;
	QLabel * labelHistoryCallNumber;
	QLabel * labelHistoryTime;
	
	
	//Automate attributes
	/**
	 *  actionPerformedStateMap[orig_state][action]
	 *  Map of the states to go to when the action action is 
	 *  performed on a call in state orig_state.
	**/
	static const call_state actionPerformedStateMap [11][5];
	
	/**
	 *  actionPerformedFunctionMap[orig_state][action]
	 *  Map of the functions to call when the action action is 
	 *  performed on a call in state orig_state.
	**/
	static const function actionPerformedFunctionMap [11][5];
	
	/**
	 *  stateChangedStateMap[orig_state][daemon_new_state]
	 *  Map of the states to go to when the daemon sends the signal 
	 *  callStateChanged with arg daemon_new_state
	 *  on a call in state orig_state.
	**/
	static const call_state stateChangedStateMap [11][6];
	
	/**
	 *  stateChangedFunctionMap[orig_state][daemon_new_state]
	 *  Map of the functions to call when the daemon sends the signal 
	 *  callStateChanged with arg daemon_new_state
	 *  on a call in state orig_state.
	**/
	static const function stateChangedFunctionMap [11][6];
	
	static const char * historyIcons[3];
	
	call_state currentState;
	bool recording;
	
	static const char * callStateIcons[11];

	Call(call_state startState, QString callId, QString peerNumber = "", QString account = "", QString peerName = "");
	
	static daemon_call_state toDaemonCallState(const QString & stateName);
	
	//Automate functions
	// See actionPerformedFunctionMap and stateChangedFunctionMap
	// to know when it is called.
	void nothing();
	void accept();
	void refuse();
	void acceptTransf();
	void acceptHold();
	void hangUp();
	void cancel();
	void hold();
	void call();
	void transfer();
	void unhold();
	void switchRecord();
	void setRecord();
	void start();
	void startWeird();
	void warning();

public:
	
	//Constructors & Destructors
	~Call();
	void initCallItem();
	void initCallItemWidget();
	static Call * buildDialingCall(QString callId, const QString & peerName, QString account = "");
	static Call * buildIncomingCall(const QString & callId);
	static Call * buildRingingCall(const QString & callId);
	static Call * buildHistoryCall(const QString & callId, uint startTimeStamp, uint stopTimeStamp, QString account, QString name, QString number, QString type);
	static Call * buildExistingCall(QString callId);
	
	static history_state getHistoryStateFromType(QString type);
	static call_state getStartStateFromDaemonCallState(QString daemonCallState, QString daemonCallType);
	static history_state getHistoryStateFromDaemonCallState(QString daemonCallState, QString daemonCallType);
	
	//Getters
	QListWidgetItem * getItem();
	QWidget * getItemWidget();
	QListWidgetItem * getHistoryItem();
	QWidget * getHistoryItemWidget();
	call_state getState() const;
	QString getCallId() const;
	QString getPeerPhoneNumber() const;
	QString getPeerName() const;
	call_state getCurrentState() const;
	history_state getHistoryState() const;
	bool getRecording() const;
	QString getAccountId() const;
	bool isHistory() const;
	
	//Automate calls
	call_state stateChanged(const QString & newState);
	call_state actionPerformed(call_action action);
	
	//Setters
	void appendItemText(QString text);
	void backspaceItemText();
	void setItemIcon(const QString pixmap);
// 	void setPeerName(const QString peerName);
	void changeCurrentState(call_state newState);
	
	//Updates
	void updateItem();

	//Utils
	Contact * findContactForNumberInKAddressBook(QString number);

};

#endif