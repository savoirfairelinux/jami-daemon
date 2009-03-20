#include "Call.h"

#include "callmanager_interface_p.h"
#include "callmanager_interface_singleton.h"
#include "SFLPhone.h"
#include "sflphone_const.h"

const call_state Call::actionPerformedStateMap [11][5] = 
{
//                      ACCEPT                  REFUSE                  TRANSFER                   HOLD                           RECORD
/*INCOMING     */  {CALL_STATE_INCOMING   , CALL_STATE_INCOMING    , CALL_STATE_ERROR        , CALL_STATE_INCOMING     ,  CALL_STATE_INCOMING     },
/*RINGING      */  {CALL_STATE_ERROR      , CALL_STATE_RINGING     , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_RINGING      },
/*CURRENT      */  {CALL_STATE_ERROR      , CALL_STATE_CURRENT     , CALL_STATE_TRANSFER     , CALL_STATE_CURRENT      ,  CALL_STATE_CURRENT      },
/*DIALING      */  {CALL_STATE_DIALING    , CALL_STATE_OVER        , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_ERROR        },
/*HOLD         */  {CALL_STATE_ERROR      , CALL_STATE_HOLD        , CALL_STATE_TRANSF_HOLD  , CALL_STATE_HOLD         ,  CALL_STATE_HOLD         },
/*FAILURE      */  {CALL_STATE_ERROR      , CALL_STATE_FAILURE     , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_ERROR        },
/*BUSY         */  {CALL_STATE_ERROR      , CALL_STATE_BUSY        , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_ERROR        },
/*TRANSFER     */  {CALL_STATE_TRANSFER   , CALL_STATE_TRANSFER    , CALL_STATE_CURRENT      , CALL_STATE_TRANSFER     ,  CALL_STATE_TRANSFER     },
/*TRANSF_HOLD  */  {CALL_STATE_TRANSF_HOLD, CALL_STATE_TRANSF_HOLD , CALL_STATE_HOLD         , CALL_STATE_TRANSF_HOLD  ,  CALL_STATE_TRANSF_HOLD  },
/*OVER         */  {CALL_STATE_ERROR      , CALL_STATE_ERROR       , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_ERROR        },
/*ERROR        */  {CALL_STATE_ERROR      , CALL_STATE_ERROR       , CALL_STATE_ERROR        , CALL_STATE_ERROR        ,  CALL_STATE_ERROR        }
};

const call_state Call::stateChangedStateMap [11][6] = 
{
//                      RINGING                  CURRENT             BUSY              HOLD                           HUNGUP           FAILURE
/*INCOMING     */  {CALL_STATE_INCOMING , CALL_STATE_CURRENT  , CALL_STATE_ERROR  , CALL_STATE_ERROR        ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*RINGING      */  {CALL_STATE_ERROR    , CALL_STATE_CURRENT  , CALL_STATE_BUSY   , CALL_STATE_ERROR        ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*CURRENT      */  {CALL_STATE_ERROR    , CALL_STATE_CURRENT  , CALL_STATE_ERROR  , CALL_STATE_HOLD         ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*DIALING      */  {CALL_STATE_RINGING  , CALL_STATE_CURRENT  , CALL_STATE_BUSY   , CALL_STATE_ERROR        ,  CALL_STATE_ERROR ,  CALL_STATE_FAILURE  },
/*HOLD         */  {CALL_STATE_ERROR    , CALL_STATE_CURRENT  , CALL_STATE_ERROR  , CALL_STATE_ERROR        ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*FAILURE      */  {CALL_STATE_ERROR    , CALL_STATE_ERROR    , CALL_STATE_ERROR  , CALL_STATE_ERROR        ,  CALL_STATE_OVER  ,  CALL_STATE_ERROR    },
/*BUSY         */  {CALL_STATE_ERROR    , CALL_STATE_ERROR    , CALL_STATE_ERROR  , CALL_STATE_ERROR        ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*TRANSFER     */  {CALL_STATE_ERROR    , CALL_STATE_ERROR    , CALL_STATE_ERROR  , CALL_STATE_TRANSF_HOLD  ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*TRANSF_HOLD  */  {CALL_STATE_ERROR    , CALL_STATE_TRANSFER , CALL_STATE_ERROR  , CALL_STATE_ERROR        ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*OVER         */  {CALL_STATE_ERROR    , CALL_STATE_ERROR    , CALL_STATE_ERROR  , CALL_STATE_ERROR        ,  CALL_STATE_ERROR ,  CALL_STATE_ERROR    },
/*ERROR        */  {CALL_STATE_ERROR    , CALL_STATE_ERROR    , CALL_STATE_ERROR  , CALL_STATE_ERROR        ,  CALL_STATE_ERROR ,  CALL_STATE_ERROR    }
};

const function Call::actionPerformedFunctionMap[11][5] = 
{ 
//                      ACCEPT               REFUSE            TRANSFER                 HOLD                  RECORD
/*INCOMING       */  {&Call::accept     , &Call::refuse   , &Call::acceptTransf   , &Call::acceptHold  ,  &Call::setRecord     },
/*RINGING        */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::nothing     ,  &Call::setRecord     },
/*CURRENT        */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::hold        ,  &Call::setRecord     },
/*DIALING        */  {&Call::call       , &Call::nothing  , &Call::nothing        , &Call::nothing     ,  &Call::nothing       },
/*HOLD           */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::unhold      ,  &Call::setRecord     },
/*FAILURE        */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::nothing     ,  &Call::nothing       },
/*BUSY           */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::nothing     ,  &Call::nothing       },
/*TRANSFERT      */  {&Call::transfer   , &Call::hangUp   , &Call::nothing        , &Call::hold        ,  &Call::setRecord     },
/*TRANSFERT_HOLD */  {&Call::transfer   , &Call::hangUp   , &Call::nothing        , &Call::unhold      ,  &Call::setRecord     },
/*OVER           */  {&Call::nothing    , &Call::nothing  , &Call::nothing        , &Call::nothing     ,  &Call::nothing       },
/*ERROR          */  {&Call::nothing    , &Call::nothing  , &Call::nothing        , &Call::nothing     ,  &Call::nothing       }
};

const QIcon Call::historyIcons[3] = {QIcon(ICON_HISTORY_INCOMING), QIcon(ICON_HISTORY_OUTGOING), QIcon(ICON_HISTORY_MISSED)};

Call::Call(call_state startState, QString callId, QString from, QString account)
{
	this->callId = callId;
	this->peer = from;
	this->item = new QListWidgetItem(from);
	this->account = account;
	this->recording = false;
	this->currentState = startState;
	this->historyItem = NULL;
}

Call::~Call()
{
	delete item;
}
	
Call * Call::buildDialingCall(QString callId)
{
	Call * call = new Call(CALL_STATE_DIALING, callId);
	call->historyState = NONE;
	return call;
}

Call * Call::buildIncomingCall(const QString & callId, const QString & from, const QString & account)
{
	Call * call = new Call(CALL_STATE_INCOMING, callId, from, account);
	call->historyState = MISSED;
	return call;
}

daemon_call_state Call::toDaemonCallState(const QString & stateName)
{
	if(stateName == QString(CALL_STATE_CHANGE_HUNG_UP))
	{
		return DAEMON_CALL_STATE_HUNG_UP;
	}
	if(stateName == QString(CALL_STATE_CHANGE_RINGING))
	{
		return DAEMON_CALL_STATE_RINGING;
	}
	if(stateName == QString(CALL_STATE_CHANGE_CURRENT))
	{
		return DAEMON_CALL_STATE_CURRENT;
	}
	if(stateName == QString(CALL_STATE_CHANGE_HOLD))
	{
		return DAEMON_CALL_STATE_HOLD;
	}
	if(stateName == QString(CALL_STATE_CHANGE_BUSY))
	{
		return DAEMON_CALL_STATE_BUSY;
	}
	if(stateName == QString(CALL_STATE_CHANGE_FAILURE))
	{
		return DAEMON_CALL_STATE_FAILURE;
	}
	qDebug() << "stateChanged signal received with unknown state.";
}

QListWidgetItem * Call::getItem()
{
	return item;
}

QListWidgetItem * Call::getHistoryItem()
{
	if(historyItem == NULL)
	{
		historyItem = new QListWidgetItem("<H1>"+peer+"</H1>");
		historyItem->setIcon(historyIcons[historyState]);
	}
	return historyItem;
}

call_state Call::getState() const
{
	return currentState;
}

call_state Call::stateChanged(const QString & newState)
{
	call_state previousState = currentState;
	daemon_call_state dcs = toDaemonCallState(newState);
	currentState = stateChangedStateMap[currentState][dcs];
	qDebug() << "Calling stateChanged " << newState << " -> " << toDaemonCallState(newState) << " on call with state " << previousState << ". Become " << currentState;
	return currentState;
}

call_state Call::actionPerformed(call_action action, QString number)
{
	call_state previousState = currentState;
	//execute the action associated with this transition
	(this->*(actionPerformedFunctionMap[currentState][action]))(number);
	//update the state
	currentState = actionPerformedStateMap[currentState][action];
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

bool Call::getRecording() const
{
	return recording;
}

/*
void Call::putRecording()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	bool daemonRecording = callManager.getIsRecording(this -> callId);
	if(daemonRecording != recording)
	{
		callManager.setRecording(this->callId);
	}
}
*/


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
	if(!account.isEmpty())
	{
		qDebug() << "Calling " << number << " with account " << account << ". callId : " << callId;
		callManager.placeCall(account, callId, number);
		this->account = account;
		this->peer = number;
		this->historyState = OUTGOING;
	}
	else
	{
		qDebug() << "Trying to call " << number << " with no account registered . callId : " << callId;
		throw "No account registered!";
		this->historyState = NONE;
	}
}

void Call::transfer(QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
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

/*
void Call::switchRecord(QString number)
{
	qDebug() << "Switching record state for call automate. callId : " << callId;
	recording = !recording;
}
*/

void Call::setRecord(QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Setting record for call. callId : " << callId;
	callManager.setRecording(callId);
	recording = !recording;
}

