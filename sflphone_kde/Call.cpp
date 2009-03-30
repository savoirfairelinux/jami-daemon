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
	labelCallNumber = new QLabel(peer, itemWidget);
	labelTransferTo = new QLabel("Transfer to : ", itemWidget);
	labelTransferNumber = new QLabel(itemWidget);
	QSpacerItem * horizontalSpacer = new QSpacerItem(16777215, 20, QSizePolicy::Preferred, QSizePolicy::Minimum);
	labelIcon->setObjectName(QString(CALL_ITEM_ICON));
	labelCallNumber->setObjectName(QString(CALL_ITEM_CALL_NUMBER));
	labelTransferTo->setObjectName(QString(CALL_ITEM_TRANSFER_LABEL));
	labelTransferNumber->setObjectName(QString(CALL_ITEM_TRANSFER_NUMBER));
	QGridLayout * layout = new QGridLayout(itemWidget);
	layout->setMargin(3);
	layout->setSpacing(3);
	layout->addWidget(labelIcon, 0, 0, 2, 1);
	layout->addWidget(labelCallNumber, 0, 1, 1, 2);
	layout->addWidget(labelTransferTo, 1, 1, 1, 1);
	layout->addWidget(labelTransferNumber, 1, 2, 1, 2);
	layout->addItem(horizontalSpacer, 0, 3, 1, 3);
	//labelIcon->raise();
	//labelCallNumber->raise();
	//labelTransferTo->raise();
	//labelTransferNumber->raise();
	//itemWidget->setLayoutDirection(Qt::LeftToRight);
	itemWidget->setLayout(layout);
	//item->setSizeHint(itemWidget->sizeHint());
	//setItemIcon(QString(ICON_REFUSE));
	updateItem();
}

void Call::setItemIcon(const QString pixmap)
{
	qDebug() << "setItemIcon(" << pixmap << ");";
	QString str(CALL_ITEM_ICON);
	qDebug() << "str = " << str;
	qDebug() << "setItemIcon1";
	//QLabel * labelIcon = itemWidget->findChild<QLabel * >(str);
	qDebug() << "setItemIcon2";
	//QPixmap icon(pixmap);
	QPixmap * icon = new QPixmap(":/images/icons/dial.svg");
	qDebug() << "setItemIcon2b";
	labelIcon->setPixmap(*icon);
	qDebug() << "setItemIcon3";
}

Call::Call(call_state startState, QString callId, QString from, QString account)
{
	for(int i = 0 ; i < 100 ; i++)
	{
		qDebug() << i << " :";
		QString str(callStateIcons[startState]);
		qDebug() << str;
	}
	qDebug() << "<<<<Done>>>>";
	this->callId = callId;
	this->peer = from;
	changeCurrentState(startState);
	initCallItem();
	this->account = account;
	this->recording = false;
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
	if(historyItem == NULL)
	{
		historyItem = new QListWidgetItem(peer);
		historyItem->setIcon(QIcon(historyIcons[historyState]));
	}
	return historyItem;
}

call_state Call::getState() const
{
	return currentState;
}

call_state Call::stateChanged(const QString & newStateName)
{
	call_state previousState = currentState;
	daemon_call_state dcs = toDaemonCallState(newStateName);
	changeCurrentState(stateChangedStateMap[currentState][dcs]);
	qDebug() << "Calling stateChanged " << newStateName << " -> " << toDaemonCallState(newStateName) << " on call with state " << previousState << ". Become " << currentState;
	return currentState;
}

call_state Call::actionPerformed(call_action action)
{
	call_state previousState = currentState;
	//execute the action associated with this transition
	(this->*(actionPerformedFunctionMap[currentState][action]))();
	//update the state
	changeCurrentState(actionPerformedStateMap[currentState][action]);
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
	QLabel * transferNumber = itemWidget->findChild<QLabel *>(QString(CALL_ITEM_TRANSFER_NUMBER));
	QString number = transferNumber->text();
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
	QLabel * callNumber = itemWidget->findChild<QLabel *>(QString(CALL_ITEM_CALL_NUMBER));
	QString number = callNumber->text();
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

void Call::transfer()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	QLabel * transferNumber = itemWidget->findChild<QLabel *>(QString(CALL_ITEM_TRANSFER_NUMBER));
	QString number = transferNumber->text();
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

void Call::appendItemText(QString text)
{
	if(currentState == CALL_STATE_TRANSFER || currentState == CALL_STATE_TRANSF_HOLD)
	{
		QLabel * transferNumber = itemWidget->findChild<QLabel *>(QString(CALL_ITEM_TRANSFER_NUMBER));
		transferNumber->setText(transferNumber->text() + text);
	}
	else
	{
		QLabel * callNumber = itemWidget->findChild<QLabel *>(QString(CALL_ITEM_CALL_NUMBER));
		callNumber->setText(callNumber->text() + text);
	}
}

void Call::changeCurrentState(call_state newState)
{
	currentState = newState;
	updateItem();
}

void Call::updateItem()
{
	qDebug() << callStateIcons[currentState];
	qDebug() << "updateItem0";
	QString str(callStateIcons[currentState]);
	qDebug() << "updateItem1";
	setItemIcon(str);
	qDebug() << "updateItem2";
	bool transfer = currentState == CALL_STATE_TRANSFER || currentState == CALL_STATE_TRANSF_HOLD;
	qDebug() << "updateItem3";
	qDebug() << "transfer : " << transfer;
	qDebug() << "updateItem4";
	QLabel * transferLabel = itemWidget->findChild<QLabel *>(QString(CALL_ITEM_TRANSFER_LABEL));
	qDebug() << "updateItem5";
	QLabel * transferNumber = itemWidget->findChild<QLabel *>(QString(CALL_ITEM_TRANSFER_NUMBER));
	qDebug() << "updateItem6";
	transferLabel->setVisible(transfer);
	qDebug() << "updateItem7";
	transferNumber->setVisible(transfer);
	qDebug() << "updateItem8";
	if(!transfer)
		transferNumber->setText("");
	qDebug() << "updateItem9";
}
