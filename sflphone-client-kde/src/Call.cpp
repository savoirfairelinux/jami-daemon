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
#include "Call.h"

#include "callmanager_interface_singleton.h"
#include "SFLPhone.h"
#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"

#include <kabc/addressbook.h>
#include <kabc/stdaddressbook.h>


using namespace KABC;

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


const function Call::actionPerformedFunctionMap[11][5] = 
{ 
//                      ACCEPT               REFUSE            TRANSFER                 HOLD                  RECORD
/*INCOMING       */  {&Call::accept     , &Call::refuse   , &Call::acceptTransf   , &Call::acceptHold  ,  &Call::setRecord     },
/*RINGING        */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::nothing     ,  &Call::setRecord     },
/*CURRENT        */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::hold        ,  &Call::setRecord     },
/*DIALING        */  {&Call::call       , &Call::cancel  , &Call::nothing        , &Call::nothing     ,  &Call::nothing       },
/*HOLD           */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::unhold      ,  &Call::setRecord     },
/*FAILURE        */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::nothing     ,  &Call::nothing       },
/*BUSY           */  {&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::nothing     ,  &Call::nothing       },
/*TRANSFERT      */  {&Call::transfer   , &Call::hangUp   , &Call::nothing        , &Call::hold        ,  &Call::setRecord     },
/*TRANSFERT_HOLD */  {&Call::transfer   , &Call::hangUp   , &Call::nothing        , &Call::unhold      ,  &Call::setRecord     },
/*OVER           */  {&Call::nothing    , &Call::nothing  , &Call::nothing        , &Call::nothing     ,  &Call::nothing       },
/*ERROR          */  {&Call::nothing    , &Call::nothing  , &Call::nothing        , &Call::nothing     ,  &Call::nothing       }
};


const call_state Call::stateChangedStateMap [11][6] = 
{
//                      RINGING                  CURRENT             BUSY              HOLD                           HUNGUP           FAILURE
/*INCOMING     */ {CALL_STATE_INCOMING    , CALL_STATE_CURRENT  , CALL_STATE_BUSY   , CALL_STATE_HOLD         ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*RINGING      */ {CALL_STATE_RINGING     , CALL_STATE_CURRENT  , CALL_STATE_BUSY   , CALL_STATE_HOLD         ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*CURRENT      */ {CALL_STATE_CURRENT     , CALL_STATE_CURRENT  , CALL_STATE_BUSY   , CALL_STATE_HOLD         ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*DIALING      */ {CALL_STATE_RINGING     , CALL_STATE_CURRENT  , CALL_STATE_BUSY   , CALL_STATE_HOLD         ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*HOLD         */ {CALL_STATE_HOLD        , CALL_STATE_CURRENT  , CALL_STATE_BUSY   , CALL_STATE_HOLD         ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*FAILURE      */ {CALL_STATE_FAILURE     , CALL_STATE_FAILURE  , CALL_STATE_BUSY   , CALL_STATE_FAILURE      ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*BUSY         */ {CALL_STATE_BUSY        , CALL_STATE_CURRENT  , CALL_STATE_BUSY   , CALL_STATE_BUSY         ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*TRANSFER     */ {CALL_STATE_TRANSFER    , CALL_STATE_TRANSFER , CALL_STATE_BUSY   , CALL_STATE_TRANSF_HOLD  ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*TRANSF_HOLD  */ {CALL_STATE_TRANSF_HOLD , CALL_STATE_TRANSFER , CALL_STATE_BUSY   , CALL_STATE_TRANSF_HOLD  ,  CALL_STATE_OVER  ,  CALL_STATE_FAILURE  },
/*OVER         */ {CALL_STATE_OVER        , CALL_STATE_OVER     , CALL_STATE_OVER   , CALL_STATE_OVER         ,  CALL_STATE_OVER  ,  CALL_STATE_OVER     },
/*ERROR        */ {CALL_STATE_ERROR       , CALL_STATE_ERROR    , CALL_STATE_ERROR  , CALL_STATE_ERROR        ,  CALL_STATE_ERROR ,  CALL_STATE_ERROR    }
};

const function Call::stateChangedFunctionMap[11][6] = 
{ 
//                      RINGING                  CURRENT             BUSY              HOLD                    HUNGUP           FAILURE
/*INCOMING       */  {&Call::nothing    , &Call::start     , &Call::startWeird     , &Call::startWeird   ,  &Call::start        , &Call::start  },
/*RINGING        */  {&Call::nothing    , &Call::start     , &Call::start          , &Call::start        ,  &Call::start        , &Call::start  },
/*CURRENT        */  {&Call::nothing    , &Call::nothing   , &Call::warning        , &Call::nothing      ,  &Call::nothing      , &Call::nothing },
/*DIALING        */  {&Call::nothing    , &Call::warning   , &Call::warning        , &Call::warning      ,  &Call::warning      , &Call::warning },
/*HOLD           */  {&Call::nothing    , &Call::nothing   , &Call::warning        , &Call::nothing      ,  &Call::nothing      , &Call::nothing },
/*FAILURE        */  {&Call::nothing    , &Call::warning   , &Call::warning        , &Call::warning      ,  &Call::nothing      , &Call::nothing },
/*BUSY           */  {&Call::nothing    , &Call::nothing   , &Call::nothing        , &Call::warning      ,  &Call::nothing      , &Call::nothing },
/*TRANSFERT      */  {&Call::nothing    , &Call::nothing   , &Call::warning        , &Call::nothing      ,  &Call::nothing      , &Call::nothing },
/*TRANSFERT_HOLD */  {&Call::nothing    , &Call::nothing   , &Call::warning        , &Call::nothing      ,  &Call::nothing      , &Call::nothing },
/*OVER           */  {&Call::nothing    , &Call::warning   , &Call::warning        , &Call::warning      ,  &Call::nothing      , &Call::warning },
/*ERROR          */  {&Call::nothing    , &Call::nothing   , &Call::nothing        , &Call::nothing      ,  &Call::nothing      , &Call::nothing }
};


const char * Call::callStateIcons[11] = {ICON_INCOMING, ICON_RINGING, ICON_CURRENT, ICON_DIALING, ICON_HOLD, ICON_FAILURE, ICON_BUSY, ICON_TRANSFER, ICON_TRANSF_HOLD, "", ""};

const char * Call::historyIcons[3] = {ICON_HISTORY_INCOMING, ICON_HISTORY_OUTGOING, ICON_HISTORY_MISSED};

void Call::initCallItem()
{
	qDebug() << "initCallItem";
	item = new QListWidgetItem();
	item->setSizeHint(QSize(140,45));
	item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled|Qt::ItemIsEnabled);
	
	itemWidget = new QWidget();
	labelIcon = new QLabel();
	labelCallNumber = new QLabel(peerPhoneNumber);
	labelTransferPrefix = new QLabel(i18n("Transfer to : "));
	labelTransferNumber = new QLabel();
	QSpacerItem * horizontalSpacer = new QSpacerItem(16777215, 20, QSizePolicy::Preferred, QSizePolicy::Minimum);
	
	QHBoxLayout * mainLayout = new QHBoxLayout();
	mainLayout->setContentsMargins ( 3, 1, 2, 1);
	mainLayout->setSpacing(4);
	QVBoxLayout * descr = new QVBoxLayout();
	descr->setMargin(1);
	descr->setSpacing(1);
	QHBoxLayout * transfer = new QHBoxLayout();
	transfer->setMargin(0);
	transfer->setSpacing(0);
	mainLayout->addWidget(labelIcon);
	if(! peerName.isEmpty())
	{
		labelPeerName = new QLabel(peerName);
		descr->addWidget(labelPeerName);
	}
	descr->addWidget(labelCallNumber);
	transfer->addWidget(labelTransferPrefix);
	transfer->addWidget(labelTransferNumber);
	descr->addLayout(transfer);
	mainLayout->addLayout(descr);
	mainLayout->addItem(horizontalSpacer);

	itemWidget->setLayout(mainLayout);
}

void Call::setItemIcon(const QString pixmap)
{
	labelIcon->setPixmap(QPixmap(pixmap));
}


Call::Call(call_state startState, QString callId, QString peerName, QString peerNumber, QString account)
{
	this->callId = callId;
	this->peerPhoneNumber = peerNumber;
	this->peerName = peerName;
	initCallItem();
	changeCurrentState(startState);
	this->account = account;
	this->recording = false;
	this->historyItem = NULL;
	this->historyItemWidget = NULL;
	this->startTime = NULL;
	this->stopTime = NULL;
}

Call * Call::buildExistingCall(QString callId)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	MapStringString details = callManager.getCallDetails(callId).value();
	qDebug() << "Constructing existing call with details : " << details;
	QString peerNumber = details[CALL_PEER_NUMBER];
	QString peerName = details[CALL_PEER_NAME];
	call_state startState = getStartStateFromDaemonCallState(details[CALL_STATE], details[CALL_TYPE]);
	QString account = details[CALL_ACCOUNTID];
	Call * call = new Call(startState, callId, peerName, peerNumber, account);
	call->startTime = new QDateTime(QDateTime::currentDateTime());
	call->recording = callManager.getIsRecording(callId);
	call->historyState = getHistoryStateFromDaemonCallState(details[CALL_STATE], details[CALL_TYPE]);
	return call;
}

Call::~Call()
{
	delete startTime;
	delete stopTime;
	delete item;
	//delete itemWidget;
	delete historyItem;
	//delete historyItemWidget;
}
	
Call * Call::buildDialingCall(QString callId, const QString & peerName, QString account)
{
	Call * call = new Call(CALL_STATE_DIALING, callId, peerName, "", account);
	call->historyState = NONE;
	return call;
}

Call * Call::buildIncomingCall(const QString & callId/*, const QString & from, const QString & account*/)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	MapStringString details = callManager.getCallDetails(callId).value();
	qDebug() << "details = " << details;
	QString from = details[CALL_PEER_NUMBER];
	QString account = details[CALL_ACCOUNTID];
	QString peerName = details[CALL_PEER_NAME];
	Call * call = new Call(CALL_STATE_INCOMING, callId, peerName, from, account);
	call->historyState = MISSED;
	return call;
}

Call * Call::buildRingingCall(const QString & callId)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	MapStringString details = callManager.getCallDetails(callId).value();
	QString from = details[CALL_PEER_NUMBER];
	QString account = details[CALL_ACCOUNTID];
	QString peerName = details[CALL_PEER_NAME];
	Call * call = new Call(CALL_STATE_RINGING, callId, peerName, from, account);
	call->historyState = OUTGOING;
	return call;
}

Call * Call::buildHistoryCall(const QString & callId, uint startTimeStamp, uint stopTimeStamp, QString account, QString name, QString number, QString type)
{
	if(name == "empty") name = "";
	Call * call = new Call(CALL_STATE_OVER, callId, name, number, account);
	call->startTime = new QDateTime(QDateTime::fromTime_t(startTimeStamp));
	call->stopTime = new QDateTime(QDateTime::fromTime_t(stopTimeStamp));
	call->historyState = getHistoryStateFromType(type);
	return call;
}


history_state Call::getHistoryStateFromType(QString type)
{
	if(type == DAEMON_HISTORY_TYPE_MISSED)
	{
		return MISSED;
	}
	else if(type == DAEMON_HISTORY_TYPE_OUTGOING)
	{
		return OUTGOING;
	}
	else if(type == DAEMON_HISTORY_TYPE_INCOMING)
	{
		return INCOMING;
	}
	return NONE;
}

call_state Call::getStartStateFromDaemonCallState(QString daemonCallState, QString daemonCallType)
{
	if(daemonCallState == DAEMON_CALL_STATE_INIT_CURRENT)
	{
		return CALL_STATE_CURRENT;
	}
	else if(daemonCallState == DAEMON_CALL_STATE_INIT_HOLD)
	{
		return CALL_STATE_HOLD;
	}
	else if(daemonCallState == DAEMON_CALL_STATE_INIT_BUSY)
	{
		return CALL_STATE_BUSY;
	}
	else if(daemonCallState == DAEMON_CALL_STATE_INIT_INACTIVE && daemonCallType == DAEMON_CALL_TYPE_INCOMING)
	{
		return CALL_STATE_INCOMING;
	}
	else if(daemonCallState == DAEMON_CALL_STATE_INIT_INACTIVE && daemonCallType == DAEMON_CALL_TYPE_OUTGOING)
	{
		return CALL_STATE_RINGING;
	}
	else if(daemonCallState == DAEMON_CALL_STATE_INIT_INCOMING)
	{
		return CALL_STATE_INCOMING;
	}
	else if(daemonCallState == DAEMON_CALL_STATE_INIT_RINGING)
	{
		return CALL_STATE_RINGING;
	}
	else
	{
		return CALL_STATE_FAILURE;
	}
}

history_state Call::getHistoryStateFromDaemonCallState(QString daemonCallState, QString daemonCallType)
{
	if((daemonCallState == DAEMON_CALL_STATE_INIT_CURRENT || daemonCallState == DAEMON_CALL_STATE_INIT_HOLD) && daemonCallType == DAEMON_CALL_TYPE_INCOMING)
	{
		return INCOMING;
	}
	else if((daemonCallState == DAEMON_CALL_STATE_INIT_CURRENT || daemonCallState == DAEMON_CALL_STATE_INIT_HOLD) && daemonCallType == DAEMON_CALL_TYPE_OUTGOING)
	{
		return OUTGOING;
	}
	else if(daemonCallState == DAEMON_CALL_STATE_INIT_BUSY)
	{
		return OUTGOING;
	}
	else if(daemonCallState == DAEMON_CALL_STATE_INIT_INACTIVE && daemonCallType == DAEMON_CALL_TYPE_INCOMING)
	{
		return INCOMING;
	}
	else if(daemonCallState == DAEMON_CALL_STATE_INIT_INACTIVE && daemonCallType == DAEMON_CALL_TYPE_OUTGOING)
	{
		return MISSED;
	}
	else
	{
		return NONE;
	}
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
	if(stateName == QString(CALL_STATE_CHANGE_UNHOLD_CURRENT))
	{
		return DAEMON_CALL_STATE_CURRENT;
	}
	if(stateName == QString(CALL_STATE_CHANGE_UNHOLD_RECORD))
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
	return DAEMON_CALL_STATE_FAILURE;
}


Contact * Call::findContactForNumberInKAddressBook(QString number)
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	MapStringInt addressBookSettings = configurationManager.getAddressbookSettings().value();
	bool displayPhoto = addressBookSettings[ADDRESSBOOK_DISPLAY_CONTACT_PHOTO];
	AddressBook * ab = KABC::StdAddressBook::self(true);
	QVector<Contact *> results = QVector<Contact *>();
	AddressBook::Iterator it;
	for ( it = ab->begin(); it != ab->end(); ++it ) {	
		for(int i = 0 ; i < it->phoneNumbers().count() ; i++)
		{
			if(it->phoneNumbers().at(i) == number)
			{
				return new Contact( *it, it->phoneNumbers().at(i), displayPhoto );
			}
		}
	}
	return NULL;
}

QListWidgetItem * Call::getItem()
{
	return item;
}

QWidget * Call::getItemWidget()
{
	return itemWidget;
}

QListWidgetItem * Call::getHistoryItem()
{
	if(historyItem == NULL && historyState != NONE)
	{
		historyItem = new QListWidgetItem();
		historyItem->setSizeHint(QSize(140,45));
	}
	return historyItem;
}

QWidget * Call::getHistoryItemWidget()
{
		historyItemWidget = new QWidget();
		labelHistoryIcon = new QLabel();
		labelHistoryIcon->setPixmap(QPixmap(historyIcons[historyState]));
		labelHistoryCallNumber = new QLabel(peerPhoneNumber);
		labelHistoryTime = new QLabel(startTime->toString(Qt::LocaleDate));
		
		QSpacerItem * horizontalSpacer = new QSpacerItem(16777215, 20, QSizePolicy::Preferred, QSizePolicy::Minimum);
	
		QHBoxLayout * mainLayout = new QHBoxLayout();
		mainLayout->setContentsMargins ( 3, 1, 2, 1);
		mainLayout->setSpacing(4);
		QVBoxLayout * descr = new QVBoxLayout();
		descr->setMargin(1);
		descr->setSpacing(1);
		descr->setMargin(0);
		descr->setSpacing(1);
		mainLayout->addWidget(labelHistoryIcon);
		if(! peerName.isEmpty())
		{
			labelHistoryPeerName = new QLabel(peerName);
			descr->addWidget(labelHistoryPeerName);
		}
		descr->addWidget(labelHistoryCallNumber);
		descr->addWidget(labelHistoryTime);
		mainLayout->addLayout(descr);
		mainLayout->addItem(horizontalSpacer);
		historyItemWidget->setLayout(mainLayout);
// 	}
	return historyItemWidget;
}

call_state Call::getState() const
{
	return currentState;
}

history_state Call::getHistoryState() const
{
	return historyState;
}

bool Call::isHistory() const
{
	return (getState() == CALL_STATE_OVER);
}

call_state Call::stateChanged(const QString & newStateName)
{
	call_state previousState = currentState;
	daemon_call_state dcs = toDaemonCallState(newStateName);
	//(this->*(stateChangedFunctionMap[currentState][dcs]))();
	changeCurrentState(stateChangedStateMap[currentState][dcs]);
	(this->*(stateChangedFunctionMap[previousState][dcs]))();
	qDebug() << "Calling stateChanged " << newStateName << " -> " << toDaemonCallState(newStateName) << " on call with state " << previousState << ". Become " << currentState;
	return currentState;
}

call_state Call::actionPerformed(call_action action)
{
	call_state previousState = currentState;
	//update the state
	changeCurrentState(actionPerformedStateMap[previousState][action]);
	//execute the action associated with this transition
	(this->*(actionPerformedFunctionMap[previousState][action]))();
	qDebug() << "Calling action " << action << " on call with state " << previousState << ". Become " << currentState;
	//return the new state
	return currentState;
}

QString Call::getCallId() const
{
	return callId;
}

QString Call::getPeerPhoneNumber() const
{
	return peerPhoneNumber;
}

QString Call::getPeerName() const
{
	return peerName;
}

call_state Call::getCurrentState() const
{
	return currentState;
}

bool Call::getRecording() const
{
	return recording;
}

QString Call::getAccountId() const
{
	return account;
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


void Call::nothing()
{
}

void Call::accept()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Accepting call. callId : " << callId;
	callManager.accept(callId);
	this->startTime = new QDateTime(QDateTime::currentDateTime());
	this->historyState = INCOMING;
}

void Call::refuse()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Refusing call. callId : " << callId;
	callManager.refuse(callId);
	this->startTime = new QDateTime(QDateTime::currentDateTime());
	this->historyState = MISSED;
}

void Call::acceptTransf()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	QString number = labelTransferNumber->text();
	qDebug() << "Accepting call and transfering it to number : " << number << ". callId : " << callId;
	callManager.accept(callId);
	callManager.transfert(callId, number);
	//this->historyState = TRANSFERED;
}

void Call::acceptHold()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Accepting call and holding it. callId : " << callId;
	callManager.accept(callId);
	callManager.hold(callId);
	this->historyState = INCOMING;
}

void Call::hangUp()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Hanging up call. callId : " << callId;
	callManager.hangUp(callId);
}

void Call::cancel()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Canceling call. callId : " << callId;
	callManager.hangUp(callId);
}

void Call::hold()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Holding call. callId : " << callId;
	callManager.hold(callId);
}

void Call::call()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	QString number = labelCallNumber->text();
	qDebug() << "account = " << account;
	if(account.isEmpty())
	{
		qDebug() << "account is not set, taking the first registered.";
		this->account = SFLPhoneView::accountInUseId();
	}
	if(!account.isEmpty())
	{
		qDebug() << "Calling " << number << " with account " << account << ". callId : " << callId;
		callManager.placeCall(account, callId, number);
		this->account = account;
		this->peerPhoneNumber = number;
// 		Contact * contact = findContactForNumberInKAddressBook(peerPhoneNumber);
// 		if(contact) this->peerName = contact->getNickName();
		this->startTime = new QDateTime(QDateTime::currentDateTime());
		this->historyState = OUTGOING;
	}
	else
	{
		qDebug() << "Trying to call " << number << " with no account registered . callId : " << callId;
		this->historyState = NONE;
		throw "No account registered!";
	}
}

void Call::transfer()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	QString number = labelTransferNumber->text();
	qDebug() << "Transfering call to number : " << number << ". callId : " << callId;
	callManager.transfert(callId, number);
	this->stopTime = new QDateTime(QDateTime::currentDateTime());
}

void Call::unhold()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Unholding call. callId : " << callId;
	callManager.unhold(callId);
}

/*
void Call::switchRecord()
{
	qDebug() << "Switching record state for call automate. callId : " << callId;
	recording = !recording;
}
*/

void Call::setRecord()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "Setting record " << !recording << " for call. callId : " << callId;
	callManager.setRecording(callId);
	recording = !recording;
	updateItem();
}

void Call::start()
{
	qDebug() << "Starting call. callId : " << callId;
	this->startTime = new QDateTime(QDateTime::currentDateTime());
}

void Call::startWeird()
{
	qDebug() << "Starting call. callId : " << callId;
	this->startTime = new QDateTime(QDateTime::currentDateTime());
	qDebug() << "Warning : call " << callId << " had an unexpected transition of state at its start.";
}

void Call::warning()
{
	qDebug() << "Warning : call " << callId << " had an unexpected transition of state.";
}

void Call::appendItemText(QString text)
{
	QLabel * editNumber;
	switch(currentState)
	{
		case CALL_STATE_TRANSFER:
		case CALL_STATE_TRANSF_HOLD:
			editNumber = labelTransferNumber;
			break;
		case CALL_STATE_DIALING:
			editNumber = labelCallNumber;
			break;
		case CALL_STATE_CURRENT:
			text = QString();
			editNumber = labelCallNumber;
			break;		
		default:
			qDebug() << "Type key on call not editable. Doing nothing.";
			return;
	}
	editNumber->setText(editNumber->text() + text);
}

void Call::backspaceItemText()
{
	QLabel * editNumber;
	switch (currentState)
	{
		case CALL_STATE_TRANSFER:
		case CALL_STATE_TRANSF_HOLD:
			editNumber = labelTransferNumber;
			break;
		case CALL_STATE_DIALING:
			editNumber = labelCallNumber;
			break;
		default:
			qDebug() << "Backspace on call not editable. Doing nothing.";
			return;
	}
	QString text = editNumber->text();
	int textSize = text.size();
	if(textSize > 0)
	{
		editNumber->setText(text.remove(textSize-1, 1));
	}
	else
	{
		changeCurrentState(CALL_STATE_OVER);
	}
}

void Call::changeCurrentState(call_state newState)
{
	currentState = newState;
	updateItem();
}

void Call::updateItem()
{
	if(currentState != CALL_STATE_OVER)
	{
		if(currentState == CALL_STATE_CURRENT && recording)
			setItemIcon(ICON_CURRENT_REC);
		else
		{
			QString str = QString(callStateIcons[currentState]);
			setItemIcon(str);
		}
		bool transfer = currentState == CALL_STATE_TRANSFER || currentState == CALL_STATE_TRANSF_HOLD;
		labelTransferPrefix->setVisible(transfer);
		labelTransferNumber->setVisible(transfer);
		if(!transfer)
			labelTransferNumber->setText("");
	}
	else
	{
		qDebug() << "Updating item of call of state OVER. Doing nothing.";
	}
}

