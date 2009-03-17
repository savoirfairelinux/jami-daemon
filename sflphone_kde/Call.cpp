#include "Call.h"

#include "callmanager_interface_p.h"
#include "callmanager_interface_singleton.h"
#include "SFLPhone.h"
#include "sflphone_const.h"

const call_state Call::stateActionMap [11][5] = 
{
//                      ACCEPT                  REFUSE             TRANSFER                   HOLD                           RECORD
/*INCOMING     */  {CALL_STATE_CURRENT  , CALL_STATE_OVER  , CALL_STATE_OVER         , CALL_STATE_HOLD         ,  CALL_STATE_INCOMING     },
/*RINGING      */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_RINGING      },
/*CURRENT      */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_TRANSFER     , CALL_STATE_HOLD         ,  CALL_STATE_CURRENT      },
/*DIALING      */  {CALL_STATE_RINGING  , CALL_STATE_OVER  , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_DIALING      },
/*HOLD         */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_TRANSF_HOLD  , CALL_STATE_CURRENT      ,  CALL_STATE_HOLD         },
/*FAILURE      */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_ERROR        },
/*BUSY         */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_ERROR        },
/*TRANSFER     */  {CALL_STATE_OVER     , CALL_STATE_OVER  , CALL_STATE_CURRENT      , CALL_STATE_TRANSF_HOLD  ,  CALL_STATE_TRANSFER     },
/*TRANSF_HOLD  */  {CALL_STATE_OVER     , CALL_STATE_OVER  , CALL_STATE_HOLD         , CALL_STATE_TRANSFER     ,  CALL_STATE_TRANSF_HOLD  },
/*OVER         */  {CALL_STATE_ERROR    , CALL_STATE_ERROR , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_ERROR        },
/*ERROR        */  {CALL_STATE_ERROR    , CALL_STATE_ERROR , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_ERROR        }
};

const call_state Call::stateChangedMap [11][6] = 
{
//                      RINGING                  CURRENT             BUSY                   HOLD                           HUNGUP           FAILURE
/*INCOMING     */  {CALL_STATE_CURRENT  , CALL_STATE_OVER  , CALL_STATE_OVER         , CALL_STATE_HOLD         ,  CALL_STATE_INCOMING     },
/*RINGING      */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_RINGING      },
/*CURRENT      */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_TRANSFER     , CALL_STATE_HOLD         ,  CALL_STATE_CURRENT      },
/*DIALING      */  {CALL_STATE_RINGING  , CALL_STATE_OVER  , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_DIALING      },
/*HOLD         */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_TRANSF_HOLD  , CALL_STATE_CURRENT      ,  CALL_STATE_HOLD         },
/*FAILURE      */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_ERROR        },
/*BUSY         */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_ERROR        },
/*TRANSFER     */  {CALL_STATE_OVER     , CALL_STATE_OVER  , CALL_STATE_CURRENT      , CALL_STATE_TRANSF_HOLD  ,  CALL_STATE_TRANSFER     },
/*TRANSF_HOLD  */  {CALL_STATE_OVER     , CALL_STATE_OVER  , CALL_STATE_HOLD         , CALL_STATE_TRANSFER     ,  CALL_STATE_TRANSF_HOLD  },
/*OVER         */  {CALL_STATE_ERROR    , CALL_STATE_ERROR , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_ERROR        },
/*ERROR        */  {CALL_STATE_ERROR    , CALL_STATE_ERROR , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_ERROR        }
};

const function Call::functionMap[11][5] = 
{ 
//                      ACCEPT               REFUSE            TRANSFER                 HOLD                  RECORD
/*INCOMING       */  {&Call::accept     , &Call::refuse   , &Call::acceptTransf   , &Call::acceptHold  ,  &Call::switchRecord  },
/*RINGING        */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::nothing     ,  &Call::switchRecord  },
/*CURRENT        */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::hold        ,  &Call::setRecord     },
/*DIALING        */  {&Call::call       , &Call::nothing  , &Call::nothing        , &Call::nothing     ,  &Call::switchRecord  },
/*HOLD           */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::unhold      ,  &Call::setRecord     },
/*FAILURE        */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::nothing     ,  &Call::nothing       },
/*BUSY           */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::nothing     ,  &Call::nothing       },
/*TRANSFERT      */  {&Call::transfer   , &Call::hangUp   , &Call::nothing        , &Call::hold        ,  &Call::setRecord     },
/*TRANSFERT_HOLD */  {&Call::transfer   , &Call::hangUp   , &Call::nothing        , &Call::unhold      ,  &Call::setRecord     },
/*OVER           */  {&Call::nothing    , &Call::nothing  , &Call::nothing        , &Call::nothing     ,  &Call::nothing       },
/*ERROR          */  {&Call::nothing    , &Call::nothing  , &Call::nothing        , &Call::nothing     ,  &Call::nothing       }
};


Call::Call(call_state startState, QString callId)
{	
	this->callId = callId;
	this->item = new QListWidgetItem("");
	this->recording = false;
	this->currentState = startState;
}

Call::Call(call_state startState, QString callId, QString from, QString account)
{
	this->callId = callId;
	this->item = new QListWidgetItem(from);
	this->account = account;
	this->recording = false;
	this->currentState = startState;
}

Call::~Call()
{
	delete item;
}
	
Call * Call::buildDialingCall(QString callId)
{
	Call * call = new Call(CALL_STATE_DIALING, callId);
	return call;
}

Call * Call::buildIncomingCall(const QString & callId, const QString & from, const QString & account)
{
	Call * call = new Call(CALL_STATE_INCOMING, callId, from, account);
	return call;
}

QListWidgetItem * Call::getItem()
{
	return item;
}

call_state Call::getState() const
{
	return currentState;
}

call_state Call::stateChanged(const QString & newState)
{
	if(newState == QString(CALL_STATE_CHANGE_HUNG_UP))
	{
		this->currentState = CALL_STATE_OVER;
	}
	else if(newState == QString(CALL_STATE_CHANGE_HOLD))
	{
		this->currentState = CALL_STATE_HOLD;
	}
	else if(newState == QString(CALL_STATE_CHANGE_UNHOLD_CURRENT))
	{
		this->currentState = CALL_STATE_CURRENT;
	}
	else if(newState == QString(CALL_STATE_CHANGE_CURRENT))
	{
		this->currentState = CALL_STATE_CURRENT;
	}
	else if(newState == QString(CALL_STATE_CHANGE_RINGING))
	{
		this->currentState = CALL_STATE_RINGING;
	}
	else if(newState == QString(CALL_STATE_CHANGE_UNHOLD_RECORD))
	{
		this->currentState = CALL_STATE_CURRENT;
		this->recording = true;
	}
	return this->currentState;
}

call_state Call::action(call_action action, QString number)
{
	if(action == CALL_ACTION_STATE_CHANGED)
	{
		return stateChanged(number);
	}
	call_state previousState = currentState;
	//execute the action associated with this transition
	(this->*(functionMap[currentState][action]))(number);
	//update the state
	currentState = stateMap[currentState][action];
	qDebug() << "Calling action " << action << " on call with state " << previousState << ". Become " << currentState;
	//return the new state
	return currentState;
}

QString Call::getCallId()
{
	return callId;
}

call_state Call::getCurrentState() const
{
	return currentState;
}








/*************************************************
*************   Automate functions   *************
*************************************************/


void Call::nothing(QString number)
{
}

void Call::accept(QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Accepting call. callId : " << callId;
	callManager.accept(callId);
	this->startTime = & QDateTime::currentDateTime();
	this->historyState = INCOMING;
}

void Call::refuse(QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Refusing call. callId : " << callId;
	callManager.refuse(callId);
	this->startTime = & QDateTime::currentDateTime();
	this->historyState = MISSED;
}

void Call::acceptTransf(QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Accepting call and transfering it to number : " << number << ". callId : " << callId;
	callManager.accept(callId);
	callManager.transfert(callId, number);
	//this->historyState = TRANSFERED;
}

void Call::acceptHold(QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Accepting call and holding it. callId : " << callId;
	callManager.accept(callId);
	callManager.hold(callId);
	this->historyState = INCOMING;
}

void Call::hangUp(QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Hanging up call. callId : " << callId;
	callManager.hangUp(callId);
}

void Call::hold(QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Holding call. callId : " << callId;
	callManager.hold(callId);
}

void Call::call(QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	this->account = SFLPhone::firstAccount();
	qDebug() << "Calling " << number << " with account " << this->account << ". callId : " << callId;
	callManager.placeCall(account, callId, number);
	this->historyState = OUTGOING;
}

void Call::transfer(QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	QString account = SFLPhone::firstAccount();
	qDebug() << "Transfering call to number : " << number << ". callId : " << callId;
	callManager.transfert(callId, number);
	this->stopTime = & QDateTime::currentDateTime();
}

void Call::unhold(QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Unholding call. callId : " << callId;
	callManager.unhold(callId);
}

void Call::switchRecord(QString number)
{
	qDebug() << "Switching record state for call automate. callId : " << callId;
	recording = !recording;
}

void Call::setRecord(QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Setting record for call. callId : " << callId;
	callManager.setRecording(callId);
	recording = !recording;
}

