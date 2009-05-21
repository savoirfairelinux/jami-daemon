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
	static const call_state actionPerformedStateMap [11][5];
	static const function actionPerformedFunctionMap [11][5];
	static const call_state stateChangedStateMap [11][6];
	static const function stateChangedFunctionMap [11][6];
	
	static const char * historyIcons[3];
	
	call_state currentState;
	bool recording;
	
	static const char * callStateIcons[11];

	Call(call_state startState, QString callId, QString peerNumber = "", QString account = "", QString peerName = "");
	
	static daemon_call_state toDaemonCallState(const QString & stateName);
	
	//Automate functions
	void nothing();
	void accept();
	void refuse();
	void acceptTransf();
	void acceptHold();
	void hangUp();
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
	
	~Call();
	void initCallItem();
	static Call * buildDialingCall(QString callId, const QString & peerName);
	static Call * buildIncomingCall(const QString & callId/*, const QString & from, const QString & account*/);
	static Call * buildRingingCall(const QString & callId);
	Contact * findContactForNumberInKAddressBook(QString number);
	
	QListWidgetItem * getItem();
	QWidget * getItemWidget();
	QListWidgetItem * getHistoryItem();
	QWidget * getHistoryItemWidget();
	call_state getState() const;
	QString getCallId() const;
	QString getPeerPhoneNumber() const;
	QString getPeerName() const;
	
	call_state stateChanged(const QString & newState);
	call_state actionPerformed(call_action action);
	call_state getCurrentState() const;
	history_state getHistoryState() const;
	bool getRecording() const;
	void appendItemText(QString text);
	void backspaceItemText();
	void setItemIcon(const QString pixmap);
// 	void setPeerName(const QString peerName);
	void changeCurrentState(call_state newState);
	void updateItem();


};

#endif