
#include "Automate.h"

#include "callmanager_interface_p.h"
#include "callmanager_interface_singleton.h"
#include "SFLPhone.h"



const call_state Automate::stateMap [11][5] = 
{
//                      ACCEPT                  REFUSE             TRANSFER                   HOLD                           RECORD
/*INCOMING       */  {CALL_STATE_CURRENT  , CALL_STATE_OVER  , CALL_STATE_OVER           , CALL_STATE_HOLD           ,  CALL_STATE_INCOMING       },
/*RINGING        */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_ERROR          , CALL_STATE_ERROR          ,  CALL_STATE_RINGING        },
/*CURRENT        */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_TRANSFER       , CALL_STATE_HOLD           ,  CALL_STATE_CURRENT        },
/*DIALING        */  {CALL_STATE_RINGING  , CALL_STATE_OVER  , CALL_STATE_ERROR          , CALL_STATE_ERROR          ,  CALL_STATE_DIALING        },
/*HOLD           */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_TRANSFER_HOLD  , CALL_STATE_CURRENT        ,  CALL_STATE_HOLD           },
/*FAILURE        */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_ERROR          , CALL_STATE_ERROR          ,  CALL_STATE_ERROR          },
/*BUSY           */  {CALL_STATE_ERROR    , CALL_STATE_OVER  , CALL_STATE_ERROR          , CALL_STATE_ERROR          ,  CALL_STATE_ERROR          },
/*TRANSFER       */  {CALL_STATE_OVER     , CALL_STATE_OVER  , CALL_STATE_CURRENT        , CALL_STATE_TRANSFER_HOLD  ,  CALL_STATE_TRANSFER       },
/*TRANSFER_HOLD  */  {CALL_STATE_OVER     , CALL_STATE_OVER  , CALL_STATE_HOLD           , CALL_STATE_TRANSFER       ,  CALL_STATE_TRANSFER_HOLD  },
/*OVER           */  {CALL_STATE_ERROR    , CALL_STATE_ERROR , CALL_STATE_ERROR          , CALL_STATE_ERROR          ,  CALL_STATE_ERROR          },
/*ERROR          */  {CALL_STATE_ERROR    , CALL_STATE_ERROR , CALL_STATE_ERROR          , CALL_STATE_ERROR          ,  CALL_STATE_ERROR          }
};

const function Automate::functionMap[11][5] = 
{ 
//                      ACCEPT                    REFUSE                TRANSFER                   HOLD
/*INCOMING       */  {&Automate::accept     , &Automate::refuse   , &Automate::acceptTransf   , &Automate::acceptHold  ,  &Automate::switchRecord  },
/*RINGING        */  {&Automate::nothing    , &Automate::hangUp   , &Automate::nothing        , &Automate::nothing     ,  &Automate::switchRecord  },
/*CURRENT        */  {&Automate::nothing    , &Automate::hangUp   , &Automate::nothing        , &Automate::hold        ,  &Automate::setRecord     },
/*DIALING        */  {&Automate::call       , &Automate::nothing  , &Automate::nothing        , &Automate::nothing     ,  &Automate::switchRecord  },
/*HOLD           */  {&Automate::nothing    , &Automate::hangUp   , &Automate::nothing        , &Automate::unhold      ,  &Automate::setRecord     },
/*FAILURE        */  {&Automate::nothing    , &Automate::hangUp   , &Automate::nothing        , &Automate::nothing     ,  &Automate::nothing       },
/*BUSY           */  {&Automate::nothing    , &Automate::hangUp   , &Automate::nothing        , &Automate::nothing     ,  &Automate::nothing       },
/*TRANSFERT      */  {&Automate::transfer   , &Automate::hangUp   , &Automate::nothing        , &Automate::hold        ,  &Automate::setRecord     },
/*TRANSFERT_HOLD */  {&Automate::transfer   , &Automate::hangUp   , &Automate::nothing        , &Automate::unhold      ,  &Automate::setRecord     },
/*OVER           */  {&Automate::nothing    , &Automate::nothing  , &Automate::nothing        , &Automate::nothing     ,  &Automate::nothing       },
/*ERROR          */  {&Automate::nothing    , &Automate::nothing  , &Automate::nothing        , &Automate::nothing     ,  &Automate::nothing       }
};

call_state Automate::action(call_action action, QString callId, QString number)
{
	call_state previousState = currentState;
	//execute the action associated with this transition
	(this->*(functionMap[currentState][action]))(callId, number);
	//update the state
	currentState = stateMap[currentState][action];
	qDebug() << "Calling action " << action << " on call with state " << previousState << ". Become " << currentState;
	//return the new state
	return currentState;
}

Automate::Automate(call_state startState)
{
	//this->parent = parent;
	recording = false;
	currentState = startState;
}

call_state Automate::getCurrentState() const
{
	return currentState;
}

void Automate::nothing(QString callId, QString number)
{
}

void Automate::accept(QString callId, QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Accepting call. callId : " << callId;
	callManager.accept(callId);
}

void Automate::refuse(QString callId, QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Refusing call. callId : " << callId;
	callManager.refuse(callId);
}

void Automate::acceptTransf(QString callId, QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Accepting call and transfering it to number : " << number << ". callId : " << callId;
	callManager.accept(callId);
	callManager.transfert(callId, number);
}

void Automate::acceptHold(QString callId, QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Accepting call and holding it. callId : " << callId;
	callManager.accept(callId);
	callManager.hold(callId);
}

void Automate::hangUp(QString callId, QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Hanging up call. callId : " << callId;
	callManager.hangUp(callId);
}

void Automate::hold(QString callId, QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Holding call. callId : " << callId;
	callManager.hold(callId);
}

void Automate::call(QString callId, QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	QString account = SFLPhone::firstAccount();
	qDebug() << "Calling " << number << " with account " << account << ". callId : " << callId;
	callManager.placeCall(account, callId, number);
}

void Automate::transfer(QString callId, QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	QString account = SFLPhone::firstAccount();
	qDebug() << "Transfering call to number : " << number << ". callId : " << callId;
	callManager.transfert(callId, number);
}

void Automate::unhold(QString callId, QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Unholding call. callId : " << callId;
	callManager.unhold(callId);
}

void Automate::switchRecord(QString callId, QString number)
{
	qDebug() << "Switching record state for call automate. callId : " << callId;
	recording = !recording;
}

void Automate::setRecord(QString callId, QString number)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Setting record for call. callId : " << callId;
	callManager.unhold(callId);
}

