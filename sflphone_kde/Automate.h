#ifndef AUTOMATE_H
#define AUTOMATE_H

#include <QtGui>
//#include "Call.h"

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


class Automate;

typedef  void (Automate::*function)(QString callId, QString number);

//class Call;

class Automate
{
private:

	static const call_state stateMap [11][5];

	static const function functionMap[11][5];

	//Call * parent;
	call_state currentState;
	bool recording;
	
public:

	Automate(call_state startState);
	call_state action(call_action action, QString callId, QString number = NULL);
	call_state getCurrentState() const;

	void nothing(QString callId, QString number);
	void accept(QString callId, QString number);
	void refuse(QString callId, QString number);
	void acceptTransf(QString callId, QString number);
	void acceptHold(QString callId, QString number);
	void hangUp(QString callId, QString number);
	void hold(QString callId, QString number);
	void call(QString callId, QString number);
	void transfer(QString callId, QString number);
	void unhold(QString callId, QString number);
	void switchRecord(QString callId, QString number);
	void setRecord(QString callId, QString number);

};




#endif