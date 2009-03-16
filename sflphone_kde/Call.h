#ifndef CALL_H
#define CALL_H
#include <QtGui>
#include "Account.h"

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
   CALL_STATE_TRANSFER_HOLD,
   /** Call is over and should not be used */
   CALL_STATE_OVER,
   /** This state should never be reached */
   CALL_STATE_ERROR
} call_state;

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
   CALL_ACTION_RECORD
} call_action;


class Call;

typedef  void (Call::*function)(QString number);

class Call
{
private:

	//Call attributes
	Account * account;
	QString callId;
	QString from;
	QString to;
//	HistoryState * historyState;
	QTime start;
	QTime stop;
	QListWidgetItem * item;
	//Automate * automate;
	
	//Automate attributes
	static const call_state stateMap [11][5];
	static const function functionMap[11][5];
	call_state currentState;
	bool recording;

	Call(call_state startState, QString callId);
	Call(call_state startState, QString callId, QString from, Account & account);
	
	//Automate functions
	void nothing(QString number);
	void accept(QString number);
	void refuse(QString number);
	void acceptTransf(QString number);
	void acceptHold(QString number);
	void hangUp(QString number);
	void hold(QString number);
	void call(QString number);
	void transfer(QString number);
	void unhold(QString number);
	void switchRecord(QString number);
	void setRecord(QString number);

public:
	
	~Call();
	static Call * buildDialingCall(QString callId);
	static Call * buildIncomingCall(QString callId, QString from, Account & account);
	QListWidgetItem * getItem();
	call_state getState() const;
	QString getCallId();
	call_state action(call_action action, QString number = NULL);
	call_state getCurrentState() const;

};

#endif