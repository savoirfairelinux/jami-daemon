#include "Call.h"

#include "callmanager_interface_p.h"
#include "callmanager_interface_singleton.h"
#include "SFLPhone.h"
#include "sflphone_const.h"
#include "configurationmanager_interface_p.h"
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
/*DIALING        */  {&Call::call       , &Call::nothing  , &Call::nothing        , &Call::nothing     ,  &Call::nothing       },
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

const function Call::stateChangedFunctionMap[11][6] = 
{ 
//                      RINGING                  CURRENT             BUSY              HOLD                    HUNGUP           FAILURE
/*INCOMING       */  {&Call::nothing    , &Call::start     , &Call::start          , &Call::start        ,  &Call::start        , &Call::start  },
/*RINGING        */  {&Call::nothing    , &Call::start     , &Call::start          , &Call::start        ,  &Call::start        , &Call::start  },
/*CURRENT        */  {&Call::nothing    , &Call::nothing   , &Call::nothing        , &Call::nothing      ,  &Call::nothing      , &Call::nothing },
/*DIALING        */  {&Call::nothing    , &Call::nothing   , &Call::nothing        , &Call::nothing      ,  &Call::nothing      , &Call::nothing },
/*HOLD           */  {&Call::nothing    , &Call::nothing   , &Call::nothing        , &Call::nothing      ,  &Call::nothing      , &Call::nothing },
/*FAILURE        */  {&Call::nothing    , &Call::nothing   , &Call::nothing        , &Call::nothing      ,  &Call::nothing      , &Call::nothing },
/*BUSY           */  {&Call::nothing    , &Call::nothing   , &Call::nothing        , &Call::nothing      ,  &Call::nothing      , &Call::nothing },
/*TRANSFERT      */  {&Call::nothing    , &Call::nothing   , &Call::nothing        , &Call::nothing      ,  &Call::nothing      , &Call::nothing },
/*TRANSFERT_HOLD */  {&Call::nothing    , &Call::nothing   , &Call::nothing        , &Call::nothing      ,  &Call::nothing      , &Call::nothing },
/*OVER           */  {&Call::nothing    , &Call::nothing   , &Call::nothing        , &Call::nothing      ,  &Call::nothing      , &Call::nothing },
/*ERROR          */  {&Call::nothing    , &Call::nothing   , &Call::nothing        , &Call::nothing      ,  &Call::nothing      , &Call::nothing }
};


const char * Call::callStateIcons[11] = {ICON_INCOMING, ICON_RINGING, ICON_CURRENT, ICON_DIALING, ICON_HOLD, ICON_FAILURE, ICON_BUSY, ICON_TRANSFER, ICON_TRANSF_HOLD, "", ""};

const char * Call::historyIcons[3] = {ICON_HISTORY_INCOMING, ICON_HISTORY_OUTGOING, ICON_HISTORY_MISSED};

void Call::initCallItem()
{
	qDebug() << "initCallItem";
	item = new QListWidgetItem();
	item->setSizeHint(QSize(140,30));
	item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled|Qt::ItemIsEnabled);
	
	itemWidget = new QWidget();
	labelIcon = new QLabel(itemWidget);
	qDebug() << "labelIcon : " << labelIcon;
	labelCallNumber = new QLabel(peerPhoneNumber, itemWidget);
	labelPeerName = NULL;
	labelTransferPrefix = new QLabel("Transfer to : ", itemWidget);
	labelTransferNumber = new QLabel(itemWidget);
	QSpacerItem * horizontalSpacer = new QSpacerItem(16777215, 20, QSizePolicy::Preferred, QSizePolicy::Minimum);
	QGridLayout * layout = new QGridLayout(itemWidget);
	layout->setMargin(3);
	layout->setSpacing(3);
	layout->addWidget(labelIcon, 0, 0, 2, 1);
	layout->addWidget(labelCallNumber, 0, 1, 1, 2);
	layout->addWidget(labelTransferPrefix, 1, 1, 1, 1);
	layout->addWidget(labelTransferNumber, 1, 2, 1, 2);
	layout->addItem(horizontalSpacer, 0, 3, 1, 3);
	itemWidget->setLayout(layout);
}

void Call::setItemIcon(const QString pixmap)
{
	labelIcon->setPixmap(QPixmap(pixmap));
}

void Call::setPeerName(const QString peerName)
{
	this->peerName = peerName;
	if(!labelPeerName) labelPeerName = new QLabel(peerName + " : ");
	labelPeerName->setText(peerName + " : ");
}

Call::Call(call_state startState, QString callId, QString from, QString account)
{
	this->callId = callId;
	this->peerPhoneNumber = from;
	initCallItem();
	changeCurrentState(startState);
	this->account = account;
	this->recording = false;
	this->historyItem = NULL;
	this->historyItemWidget = NULL;
	this->startTime = NULL;
	this->stopTime = NULL;
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

Call * Call::buildRingingCall(const QString & callId)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	MapStringString details = callManager.getCallDetails(callId).value();
	//QString from = details[CALL_FROM];
	//QString from = details[CALL_ACCOUNT];
	//Call * call = new Call(CALL_STATE_RINGING, callId, from, account);
	Call * call = new Call(CALL_STATE_RINGING, callId);
	call->historyState = OUTGOING;
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
	AddressBook * ab = KABC::StdAddressBook::self();
	QVector<Contact *> results = QVector<Contact *>();
	AddressBook::Iterator it;
	for ( it = ab->begin(); it != ab->end(); ++it ) {	
		for(int i = 0 ; i < it->phoneNumbers().count() ; i++)
		{
			if(it->phoneNumbers().at(i) == number)
			{
				return new Contact( *it, it->phoneNumbers().at(i).number() );
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
		historyItem->setSizeHint(QSize(140,30));
		qDebug() << "historystate = " << historyState;
	}
	return historyItem;
}

QWidget * Call::getHistoryItemWidget()
{
	if(historyItemWidget == NULL && historyState != NONE)
	{
		historyItemWidget = new QWidget();
		labelHistoryIcon = new QLabel(historyItemWidget);
		labelHistoryIcon->setPixmap(QPixmap(historyIcons[historyState]));
		labelHistoryCallNumber = new QLabel(peerPhoneNumber, historyItemWidget);
		labelHistoryPeerName = NULL;
		if(!peerName.isEmpty())
			labelHistoryPeerName = new QLabel(peerName + " : ", historyItemWidget);
		labelHistoryTime = new QLabel(startTime->toString(Qt::LocaleDate), historyItemWidget);
		QSpacerItem * horizontalSpacer = new QSpacerItem(16777215, 20, QSizePolicy::Preferred, QSizePolicy::Minimum);
		QGridLayout * layout = new QGridLayout(historyItemWidget);
		layout->setMargin(3);
		layout->setSpacing(3);
		layout->addWidget(labelHistoryIcon, 0, 0, 2, 1);
		layout->addWidget(labelHistoryCallNumber, 0, 1, 1, 2);
		layout->addWidget(labelHistoryTime, 1, 1, 1, 1);
		layout->addItem(horizontalSpacer, 0, 3, 1, 3);
		historyItemWidget->setLayout(layout);
	}
	return historyItemWidget;
}

/*
layout->addWidget(labelIcon, 0, 0, 2, 1);
	layout->addWidget(labelCallNumber, 0, 1, 1, 2);
	layout->addWidget(labelTransferPrefix, 1, 1, 1, 1);
	layout->addWidget(labelTransferNumber, 1, 2, 1, 2);
	layout->addItem(horizontalSpacer, 0, 3, 1, 3);
*/
call_state Call::getState() const
{
	return currentState;
}

history_state Call::getHistoryState() const
{
	return historyState;
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
	this->account = sflphone_kdeView::firstAccountId();
	if(!account.isEmpty())
	{
		qDebug() << "Calling " << number << " with account " << account << ". callId : " << callId;
		callManager.placeCall(account, callId, number);
		this->account = account;
		this->peerPhoneNumber = number;
		Contact * contact = findContactForNumberInKAddressBook(peerPhoneNumber);
		if(contact) this->peerName = contact->getNickName();
		this->startTime = new QDateTime(QDateTime::currentDateTime());
		this->historyState = OUTGOING;
	}
	else
	{
		qDebug() << "Trying to call " << number << " with no account registered . callId : " << callId;
		throw "No account registered!";
		this->historyState = NONE;
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
	qDebug() << "Setting record for call. callId : " << callId;
	callManager.setRecording(callId);
	recording = !recording;
}

void Call::start()
{
	qDebug() << "Starting call. callId : " << callId;
	this->startTime = new QDateTime(QDateTime::currentDateTime());
}

void Call::appendItemText(QString text)
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
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
			//TODO replace account string by an Account instance and handle damn pointers to avoid detruction of Accounts
			//if(peer == account->getAccountDetail(ACCOUNT_MAILBOX))
			if(peerPhoneNumber == configurationManager.getAccountDetails(account).value()[ACCOUNT_MAILBOX])
			{
				text = QString(QChar(0x9A));
			}
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
	if(currentState == CALL_STATE_CURRENT && recording)
		setItemIcon(ICON_CURRENT_REC);
	else
	{
		QString str(callStateIcons[currentState]);
		setItemIcon(str);
	}
	bool transfer = currentState == CALL_STATE_TRANSFER || currentState == CALL_STATE_TRANSF_HOLD;
	labelTransferPrefix->setVisible(transfer);
	labelTransferNumber->setVisible(transfer);
	if(!transfer)
		labelTransferNumber->setText("");
}
