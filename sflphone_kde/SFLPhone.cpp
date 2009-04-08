#include "SFLPhone.h"

#include <QtGui/QContextMenuEvent>

#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"
#include "callmanager_interface_singleton.h"
#include "instance_interface_singleton.h"
#include "ActionSetAccountFirst.h"

ConfigurationDialog * SFLPhone::configDialog;

SFLPhone::SFLPhone(QMainWindow *parent) : QMainWindow(parent)
{
	setupUi(this);
	
	errorWindow = new QErrorMessage(this);
	callList = new CallList();
	configDialog = new ConfigurationDialog(this);
	configDialog->setModal(true);
	
	wizard = new AccountWizard(this);
	wizard->setModal(false);
	
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	connect(&callManager, SIGNAL(callStateChanged(const QString &, const QString &)),
	        this,         SLOT(on1_callStateChanged(const QString &, const QString &)));
	connect(&callManager, SIGNAL(error(MapStringString)),
	        this,         SLOT(on1_error(MapStringString)));
	connect(&callManager, SIGNAL(incomingCall(const QString &, const QString &, const QString &)),
	        this,         SLOT(on1_incomingCall(const QString &, const QString &, const QString &)));
	connect(&callManager, SIGNAL(incomingMessage(const QString &, const QString &)),
	        this,         SLOT(on1_incomingMessage(const QString &, const QString &)));
	connect(&callManager, SIGNAL(voiceMailNotify(const QString &, int)),
	        this,         SLOT(on1_voiceMailNotify(const QString &, int)));
	connect(&callManager, SIGNAL(volumeChanged(const QString &, double)),
	        this,         SLOT(on1_volumeChanged(const QString &, double)));
	        
   //QDBusConnection::sessionBus().connect("org.sflphone.SFLphone", "/org/sflphone/SFLphone/CallManager", "org.sflphone.SFLphone.CallManager", "incomingCall",
   //             this, SLOT(on_incomingCall(const QString &accountID, const QString &callID, const QString &from)));

	loadWindow();

} 

SFLPhone::~SFLPhone()
{
	delete configDialog;
	delete wizard;
	delete callList;
	delete errorWindow;
	InstanceInterface & instance = InstanceInterfaceSingleton::getInstance();
	instance.Unregister(getpid());
}

void SFLPhone::loadWindow()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	action_displayVolumeControls->setChecked(configurationManager.getVolumeControls());
	action_displayDialpad->setChecked(configurationManager.getDialpad());
	updateWindowCallState();
	updateRecordButton();
	updateVolumeButton();
	updateRecordBar();
	updateVolumeBar();
	updateVolumeControls();
	updateDialpad();
	updateSearchHistory();
	updateSearchAddressBook();
}

/*QString SFLPhone::firstAccount()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	//ask for the list of accounts ids to the daemon
	QStringList accountIds = configurationManager.getAccountList().value();
	for (int i = 0; i < accountIds.size(); ++i){
		MapStringString accountDetails = configurationManager.getAccountDetails(accountIds[i]);
		if(accountDetails[QString(ACCOUNT_STATUS)] == QString(ACCOUNT_STATE_REGISTERED))
		{
			return accountIds[i];
		}
	}
	return QString();
}*/

QString SFLPhone::firstAccountId()
{
	return getAccountList()->firstRegisteredAccount()->getAccountId();
}

QVector<Account *> SFLPhone::registeredAccounts()
{
	return getAccountList()->registeredAccounts();
}

Account * SFLPhone::firstRegisteredAccount()
{
	return getAccountList()->firstRegisteredAccount();
}

AccountList * SFLPhone::getAccountList()
{
	return configDialog->getAccountList();
}


void SFLPhone::addCallToCallList(Call * call)
{
	QListWidgetItem * item = call->getItem();
	QWidget * widget = call->getItemWidget();
	if(item && widget)
	{
		listWidget_callList->addItem(item);
		listWidget_callList->setItemWidget(item, widget);
	}
}

void SFLPhone::addCallToCallHistory(Call * call)
{
	QListWidgetItem * item = call->getHistoryItem();
	QWidget * widget = call->getHistoryItemWidget();
	if(item && widget)
	{
		listWidget_callHistory->addItem(item);
		listWidget_callHistory->setItemWidget(item, widget);
	}
}

void SFLPhone::addContactToContactList(Contact * contact)
{
	QListWidgetItem * item = contact->getItem();
	QWidget * widget = contact->getItemWidget();
	if(item && widget)
	{
		listWidget_addressBook->addItem(item);
		listWidget_addressBook->setItemWidget(item, widget);
	}
}

void SFLPhone::typeString(QString str)
{
	qDebug() << "typeString";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	
	if(stackedWidget_screen->currentWidget() == page_callList)
	{
		callManager.playDTMF(str);
		QListWidgetItem * item = listWidget_callList->currentItem();
		if(!item)
		{
			qDebug() << "Typing when no item is selected. Opening an item.";
			Call * call = callList->addDialingCall();
			addCallToCallList(call);
			listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
		}
		callList->getCallByItem(listWidget_callList->currentItem())->appendItemText(str);
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		qDebug() << "In call history.";
		lineEdit_searchHistory->setText(lineEdit_searchHistory->text() + str);
	}
	if(stackedWidget_screen->currentWidget() == page_addressBook)
	{
		qDebug() << "In address book.";
		lineEdit_addressBook->setText(lineEdit_addressBook->text() + str);
	}
}

void SFLPhone::backspace()
{
	qDebug() << "backspace";
	if(stackedWidget_screen->currentWidget() == page_callList)
	{
		qDebug() << "In call list.";
		QListWidgetItem * item = listWidget_callList->currentItem();
		if(!item)
		{
			qDebug() << "Backspace when no item is selected. Doing nothing.";
		}
		else
		{
			Call * call = callList->getCallByItem(listWidget_callList->currentItem());
			if(!call)
			{
				qDebug() << "Error : Backspace on unexisting call.";
			}
			else
			{
				call->backspaceItemText();
				updateCallItem(call);
			}
		}
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		qDebug() << "In call history.";
		int textSize = lineEdit_searchHistory->text().size();
		if(textSize > 0)
		{
			lineEdit_searchHistory->setText(lineEdit_searchHistory->text().remove(textSize-1, 1));
		}
	}
	if(stackedWidget_screen->currentWidget() == page_addressBook)
	{
		qDebug() << "In address book.";
		int textSize = lineEdit_addressBook->text().size();
		if(textSize > 0)
		{
			lineEdit_addressBook->setText(lineEdit_addressBook->text().remove(textSize-1, 1));
		}
	}
}

void SFLPhone::actionb(Call * call, call_action action)
{
	try
	{
		call->actionPerformed(action);
	}
	catch(const char * msg)
	{
		errorWindow->showMessage(QString(msg));
	}
	updateCallItem(call);
	updateWindowCallState();
}

void SFLPhone::action(QListWidgetItem * item, call_action action)
{
	actionb(callList->getCallByItem(item), action);
}

/*******************************************
******** Update Display Functions **********
*******************************************/

void SFLPhone::updateCallItem(Call * call)
{
	QListWidgetItem * item = call->getItem();
	call_state state = call->getState();
	if(state == CALL_STATE_OVER)
	{
		qDebug() << "Updating call with CALL_STATE_OVER. Deleting item " << (*callList)[item]->getCallId();
		listWidget_callList->takeItem(listWidget_callList->row(item));
		addCallToCallHistory(call);
	}
}


void SFLPhone::updateWindowCallState()
{
	qDebug() << "updateWindowCallState";
	QListWidgetItem * item;
	
	bool enabledActions[6]= {true,true,true,true,true,true};
	QString buttonIconFiles[3] = {ICON_CALL, ICON_HANGUP, ICON_HOLD};
	bool transfer = false;
	//tells whether the call is in recording position
	bool recordActivated = false;
	//tells whether the call can be recorded in the state it is right now
	bool recordEnabled = false;
	
	if(stackedWidget_screen->currentWidget() == page_callList)
	{
		item = listWidget_callList->currentItem();
		if (!item)
		{
			qDebug() << "No item selected. Updating window.";
			enabledActions[1] = false;
			enabledActions[2] = false;
			enabledActions[3] = false;
			enabledActions[4] = false;
		}
		else
		{
			Call * call = (*callList)[item];
			call_state state = call->getState();
			//qDebug() << "calling getIsRecording on " << call->getCallId();
			//recordActivated = callManager.getIsRecording(call->getCallId());
			recordActivated = call->getRecording();
			switch (state)
			{
				case CALL_STATE_INCOMING:
					qDebug() << "Reached CALL_STATE_INCOMING with call " << (*callList)[item]->getCallId() << ". Updating window.";
					buttonIconFiles[0] = ICON_ACCEPT;
					buttonIconFiles[1] = ICON_REFUSE;
					break;
				case CALL_STATE_RINGING:
					qDebug() << "Reached CALL_STATE_RINGING with call " << (*callList)[item]->getCallId() << ". Updating window.";
					enabledActions[2] = false;
					enabledActions[3] = false;
					break;
				case CALL_STATE_CURRENT:
					qDebug() << "Reached CALL_STATE_CURRENT with call " << (*callList)[item]->getCallId() << ". Updating window.";
					recordEnabled = true;
					break;
				case CALL_STATE_DIALING:
					qDebug() << "Reached CALL_STATE_DIALING with call " << (*callList)[item]->getCallId() << ". Updating window.";
					enabledActions[2] = false;
					enabledActions[3] = false;
					enabledActions[4] = false;
					buttonIconFiles[0] = ICON_ACCEPT;
					break;
				case CALL_STATE_HOLD:
					qDebug() << "Reached CALL_STATE_HOLD with call " << (*callList)[item]->getCallId() << ". Updating window.";
					buttonIconFiles[2] = ICON_UNHOLD;
					break;		
				case CALL_STATE_FAILURE:
					qDebug() << "Reached CALL_STATE_FAILURE with call " << (*callList)[item]->getCallId() << ". Updating window.";
					enabledActions[0] = false;
					enabledActions[2] = false;
					enabledActions[3] = false;
					enabledActions[4] = false;
					break;
				case CALL_STATE_BUSY:
					qDebug() << "Reached CALL_STATE_BUSY with call " << (*callList)[item]->getCallId() << ". Updating window.";
					enabledActions[0] = false;
					enabledActions[2] = false;
					enabledActions[3] = false;
					enabledActions[4] = false;
				break;
				case CALL_STATE_TRANSFER:
					qDebug() << "Reached CALL_STATE_TRANSFER with call " << (*callList)[item]->getCallId() << ". Updating window.";
					buttonIconFiles[0] = ICON_EXEC_TRANSF;
					transfer = true;
					recordEnabled = true;
					break;
				case CALL_STATE_TRANSF_HOLD:
					qDebug() << "Reached CALL_STATE_TRANSF_HOLD with call " << (*callList)[item]->getCallId() << ". Updating window.";
					buttonIconFiles[0] = ICON_EXEC_TRANSF;
					buttonIconFiles[2] = ICON_UNHOLD;
					transfer = true;
					break;
				case CALL_STATE_OVER:
					qDebug() << "Error : Reached CALL_STATE_OVER with call " << (*callList)[item]->getCallId() << "!";
					break;
				case CALL_STATE_ERROR:
					qDebug() << "Error : Reached CALL_STATE_ERROR with call " << (*callList)[item]->getCallId() << "!";
					break;
				default:
					qDebug() << "Error : Reached unexisting state for call " << (*callList)[item]->getCallId() << "!";
					break;
			}
		}
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		item = listWidget_callHistory->currentItem();
		buttonIconFiles[0] = ICON_ACCEPT;
		if (!item)
		{
			qDebug() << "No item selected. Updating window.";
			enabledActions[0] = false;
			enabledActions[1] = false;
			enabledActions[2] = false;
			enabledActions[3] = false;
			enabledActions[4] = false;
		}
		else
		{
			enabledActions[1] = false;
			enabledActions[2] = false;
			enabledActions[3] = false;
			enabledActions[4] = false;
		}
		if(!lineEdit_searchHistory->text().isEmpty())
		{
			enabledActions[1] = true;
		}
	}
	if(stackedWidget_screen->currentWidget() == page_addressBook)
	{
		item = listWidget_addressBook->currentItem();
		buttonIconFiles[0] = ICON_ACCEPT;
		if (!item)
		{
			qDebug() << "No item selected. Updating window.";
			enabledActions[0] = false;
			enabledActions[1] = false;
			enabledActions[2] = false;
			enabledActions[3] = false;
			enabledActions[4] = false;
		}
		else
		{
			enabledActions[1] = false;
			enabledActions[2] = false;
			enabledActions[3] = false;
			enabledActions[4] = false;
		}
		if(!lineEdit_addressBook->text().isEmpty())
		{
			enabledActions[1] = true;
		}
	}
	action_accept->setEnabled(enabledActions[0]);
	action_refuse->setEnabled(enabledActions[1]);
	action_hold->setEnabled(enabledActions[2]);
	action_transfer->setEnabled(enabledActions[3]);
	action_record->setEnabled(enabledActions[4]);
	action_mailBox->setEnabled(enabledActions[5]);
	
	action_record->setIcon(QIcon(recordEnabled ? ICON_REC_DEL_ON : ICON_REC_DEL_OFF));
	action_accept->setIcon(QIcon(buttonIconFiles[0]));
	action_refuse->setIcon(QIcon(buttonIconFiles[1]));
	action_hold->setIcon(QIcon(buttonIconFiles[2]));
	
	action_transfer->setChecked(transfer);
	action_record->setChecked(recordActivated);
}

void SFLPhone::updateSearchHistory()
{
	qDebug() << "updateSearchHistory";
	lineEdit_searchHistory->setVisible(!lineEdit_searchHistory->text().isEmpty());
}

void SFLPhone::updateSearchAddressBook()
{
	qDebug() << "updateAddressBookSearch";
	lineEdit_addressBook->setVisible(!lineEdit_addressBook->text().isEmpty());
}

void SFLPhone::updateCallHistory()
{
	qDebug() << "updateCallHistory";
	while(listWidget_callHistory->count() > 0)
	{
		QListWidgetItem * item = listWidget_callHistory->takeItem(0);
		qDebug() << "take item " << item->text();
	}
	//listWidget_callHistory->clear();
	QString textSearched = lineEdit_searchHistory->text();
	for(int i = 0 ; i < callList->size() ; i++)
	{
		Call * call = (*callList)[i];
		qDebug() << "" << call->getCallId();
		if(call->getState() == CALL_STATE_OVER && call->getHistoryState() != NONE && call->getHistoryItem()->text().contains(textSearched))
		{
			qDebug() << "call->getItem()->text()=" << call->getHistoryItem()->text() << " contains textSearched=" << textSearched;
			addCallToCallHistory(call);
			//QListWidgetItem * historyItem = call->getHistoryItem();
			//listWidget_callHistory->addItem(historyItem);
			//listWidget_callHistory->setItemWidget(historyItem, call->getHistoryItemWidget());
		}
	}
}

void SFLPhone::updateAddressBook()
{
	qDebug() << "updateAddressBook";
	while(listWidget_addressBook->count() > 0)
	{
		QListWidgetItem * item = listWidget_addressBook->takeItem(0);
		qDebug() << "take item " << item->text();
	}
	QString textSearched = lineEdit_searchHistory->text();
	QVector<Contact *> contactsFound = findContactsInKAddressBook(textSearched);
	for(int i = 0 ; i < contactsFound.size() ; i++)
	{
		Contact * contact = contactsFound[i];
		qDebug() << "contact->getItem()->text()=" << contact->getItem()->text() << " contains textSearched=" << textSearched;
		addContactToContactList(contact);
		//QListWidgetItem * item = contact->getItem();
		//listWidget_addressBook->addItem(item);
		//listWidget_addressBook->setItemWidget(item, contact->getItemWidget());
	}
}

QVector<Contact *> SFLPhone::findContactsInKAddressBook(QString textSearched)
{
	return QVector<Contact *>();
}

void SFLPhone::updateRecordButton()
{
	qDebug() << "updateRecordButton";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	double recVol = callManager.getVolume(RECORD_DEVICE);
	if(recVol == 0.00)
	{
		toolButton_recVol->setIcon(QIcon(ICON_REC_VOL_0));
	}
	else if(recVol < 0.33)
	{
		toolButton_recVol->setIcon(QIcon(ICON_REC_VOL_1));
	}
	else if(recVol < 0.67)
	{
		toolButton_recVol->setIcon(QIcon(ICON_REC_VOL_2));
	}
	else
	{
		toolButton_recVol->setIcon(QIcon(ICON_REC_VOL_3));
	}
	if(recVol > 0)
		toolButton_recVol->setChecked(false);
}
void SFLPhone::updateVolumeButton()
{
	qDebug() << "updateVolumeButton";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	double sndVol = callManager.getVolume(SOUND_DEVICE);
	if(sndVol == 0.00)
	{
		toolButton_sndVol->setIcon(QIcon(ICON_SND_VOL_0));
	}
	else if(sndVol < 0.33)
	{
		toolButton_sndVol->setIcon(QIcon(ICON_SND_VOL_1));
	}
	else if(sndVol < 0.67)
	{
		toolButton_sndVol->setIcon(QIcon(ICON_SND_VOL_2));
	}
	else
	{
		toolButton_sndVol->setIcon(QIcon(ICON_SND_VOL_3));
	}
	if(sndVol > 0)
		toolButton_sndVol->setChecked(false);
}


void SFLPhone::updateRecordBar()
{
	qDebug() << "updateRecordBar";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	double recVol = callManager.getVolume(RECORD_DEVICE);
	slider_recVol->setValue((int)(recVol * 100));
}
void SFLPhone::updateVolumeBar()
{
	qDebug() << "updateVolumeBar";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	double sndVol = callManager.getVolume(SOUND_DEVICE);
	slider_sndVol->setValue((int)(sndVol * 100));
}

void SFLPhone::updateVolumeControls()
{
	qDebug() << "updateVolumeControls";
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	int display = configurationManager.getVolumeControls();
	widget_recVol->setVisible(display);
	widget_sndVol->setVisible(display);
}

void SFLPhone::updateDialpad()
{
	qDebug() << "updateDialpad";
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	int display = configurationManager.getDialpad();
	widget_dialpad->setVisible(display);
}



/************************************************************
************            Autoconnect             *************
************************************************************/

void SFLPhone::on_action_displayVolumeControls_toggled()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	configurationManager.setVolumeControls();
	updateVolumeControls();
}

void SFLPhone::on_action_displayDialpad_toggled()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	configurationManager.setDialpad();
	updateDialpad();
}

void SFLPhone::on_pushButton_1_clicked()      { typeString("1"); }
void SFLPhone::on_pushButton_2_clicked()      { typeString("2"); }
void SFLPhone::on_pushButton_3_clicked()      { typeString("3"); }
void SFLPhone::on_pushButton_4_clicked()      { typeString("4"); }
void SFLPhone::on_pushButton_5_clicked()      { typeString("5"); }
void SFLPhone::on_pushButton_6_clicked()      { typeString("6"); }
void SFLPhone::on_pushButton_7_clicked()      { typeString("7"); }
void SFLPhone::on_pushButton_8_clicked()      { typeString("8"); }
void SFLPhone::on_pushButton_9_clicked()      { typeString("9"); }
void SFLPhone::on_pushButton_0_clicked()      { typeString("0"); }
void SFLPhone::on_pushButton_diese_clicked()  { typeString("#"); }
void SFLPhone::on_pushButton_etoile_clicked() { typeString("*"); }

void SFLPhone::on_lineEdit_searchHistory_textChanged()
{
	qDebug() << "on_lineEdit_searchHistory_textEdited";
	updateSearchHistory();
	updateCallHistory();
	updateWindowCallState();
}

void SFLPhone::on_lineEdit_addressBook_textChanged()
{
	qDebug() << "on_lineEdit_addressBook_textEdited";
	updateSearchAddressBook();
	updateAddressBook();
	updateWindowCallState();
}

void SFLPhone::on_slider_recVol_valueChanged(int value)
{
	qDebug() << "on_slider_recVol_valueChanged(" << value << ")";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	callManager.setVolume(RECORD_DEVICE, (double)value / 100.0);
	updateRecordButton();
}

void SFLPhone::on_slider_sndVol_valueChanged(int value)
{
	qDebug() << "on_slider_sndVol_valueChanged(" << value << ")";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	callManager.setVolume(SOUND_DEVICE, (double)value / 100.0);
	updateVolumeButton();
}

void SFLPhone::on_toolButton_recVol_clicked(bool checked)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "on_toolButton_recVol_clicked().";
	if(!checked)
	{
		qDebug() << "checked";
		toolButton_recVol->setChecked(false);
		slider_recVol->setEnabled(true);
		callManager.setVolume(RECORD_DEVICE, (double)slider_recVol->value() / 100.0);
	}
	else
	{
		qDebug() << "unchecked";
		toolButton_recVol->setChecked(true);
		slider_recVol->setEnabled(false);
		callManager.setVolume(RECORD_DEVICE, 0.0);
	}
	updateRecordButton();
}

void SFLPhone::on_toolButton_sndVol_clicked(bool checked)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "on_toolButton_sndVol_clicked().";
	if(!checked)
	{
		qDebug() << "checked";
		toolButton_sndVol->setChecked(false);
		slider_sndVol->setEnabled(true);
		callManager.setVolume(SOUND_DEVICE, (double)slider_sndVol->value() / 100.0);
	}
	else
	{
		qDebug() << "unchecked";
		toolButton_sndVol->setChecked(true);
		slider_sndVol->setEnabled(false);
		callManager.setVolume(SOUND_DEVICE, 0.0);
	}
	updateVolumeButton();
}


void SFLPhone::on_listWidget_callList_currentItemChanged()
{
	qDebug() << "on_listWidget_callList_currentItemChanged";
	updateWindowCallState();
}

void SFLPhone::on_listWidget_callList_itemChanged()
{
	qDebug() << "on_listWidget_callList_itemChanged";
	stackedWidget_screen->setCurrentWidget(page_callList);
}

void SFLPhone::on_listWidget_callList_itemDoubleClicked(QListWidgetItem * item)
{
	qDebug() << "on_listWidget_callList_itemDoubleClicked";
	Call * call = callList->getCallByItem(item);
	call_state state = call->getCurrentState();
	switch(state)
	{
		case CALL_STATE_HOLD:
			actionb(call, CALL_ACTION_HOLD);
			break;
		case CALL_STATE_DIALING:
			actionb(call, CALL_ACTION_ACCEPT);
			break;
		default:
			qDebug() << "Double clicked an item with no action on double click.";
	}
}


void SFLPhone::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu(this);
	menu.addAction(action_accept);
	menu.addAction(action_refuse);
	menu.addAction(action_hold);
	menu.addAction(action_transfer);
	menu.addAction(action_record);
	//TODO accounts to choose
	menu.addSeparator();
	QVector<Account *> accounts = registeredAccounts();
	for (int i = 0 ; i < accounts.size() ; i++)
	{
		Account * account = accounts.at(i);
		QAction * action = new ActionSetAccountFirst(account, &menu);
		action->setCheckable(true);
		action->setChecked(false);
		if(account == firstRegisteredAccount())
		{
			action->setChecked(true);
		}
		connect(action, SIGNAL(setFirst(Account *)),
		        this  , SLOT(setAccountFirst(Account *)));
		menu.addAction(action);
	}
	menu.exec(event->globalPos());
	
}

void SFLPhone::setAccountFirst(Account * account)
{
	qDebug() << "setAccountFirst : " << account->getAlias();
	getAccountList()->setAccountFirst(account);
}

void SFLPhone::on_listWidget_callHistory_currentItemChanged()
{
	qDebug() << "on_listWidget_callHistory_currentItemChanged";
	updateWindowCallState();
}

void SFLPhone::on_listWidget_addressBook_currentItemChanged()
{
	qDebug() << "on_listWidget_addressBook_currentItemChanged";
	updateWindowCallState();
}

void SFLPhone::on_action_configureAccounts_triggered()
{
	configDialog->loadOptions();
	configDialog->setPage(PAGE_ACCOUNTS);
	configDialog->show();
}

void SFLPhone::on_action_configureAudio_triggered()
{
	configDialog->loadOptions();
	configDialog->setPage(PAGE_AUDIO);
	configDialog->show();
}

void SFLPhone::on_action_configureSflPhone_triggered()
{
	SFLPhone::configDialog->loadOptions();
	configDialog->setPage(PAGE_GENERAL);
	configDialog->show();
}

void SFLPhone::on_action_accountCreationWizard_triggered()
{
	wizard->show();
}
	

void SFLPhone::on_action_accept_triggered()
{
	if(stackedWidget_screen->currentWidget() == page_callList)
	{
		QListWidgetItem * item = listWidget_callList->currentItem();
		if(!item || (*callList)[item]->getState() == CALL_STATE_RINGING || (*callList)[item]->getState() == CALL_STATE_CURRENT || (*callList)[item]->getState() == CALL_STATE_HOLD || (*callList)[item]->getState() == CALL_STATE_BUSY)
		{
			qDebug() << "Calling when no item is selected or item currently ringing, current, hold or busy. Opening an item.";
			Call * call = callList->addDialingCall();
			addCallToCallList(call);
			listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
		}
		else
		{
			action(item, CALL_ACTION_ACCEPT);
		}
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		action_history->setChecked(false);
		stackedWidget_screen->setCurrentWidget(page_callList);
		Call * call = callList->addDialingCall();
		call->appendItemText(listWidget_callHistory->currentItem()->text());
		addCallToCallList(call);
		listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
		actionb(call, CALL_ACTION_ACCEPT);
	}
	if(stackedWidget_screen->currentWidget() == page_addressBook)
	{
		action_addressBook->setChecked(false);
		stackedWidget_screen->setCurrentWidget(page_callList);
		Call * call = callList->addDialingCall();
		call->appendItemText(listWidget_callHistory->currentItem()->text());
		addCallToCallList(call);
		listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
		actionb(call, CALL_ACTION_ACCEPT);
	}
}

void SFLPhone::on_action_refuse_triggered()
{
	if(stackedWidget_screen->currentWidget() == page_callList)
	{
		QListWidgetItem * item = listWidget_callList->currentItem();
		if(!item)
		{
			qDebug() << "Error : Hanging up when no item selected. Should not happen.";
		}
		else
		{
			action(item, CALL_ACTION_REFUSE);
		}
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		lineEdit_searchHistory->clear();
	}
	if(stackedWidget_screen->currentWidget() == page_addressBook)
	{
		lineEdit_addressBook->clear();
	}
}

void SFLPhone::on_action_hold_triggered()
{
	QListWidgetItem * item = listWidget_callList->currentItem();
	if(!item)
	{
		qDebug() << "Error : Holding when no item selected. Should not happen.";
	}
	else
	{
		action(item, CALL_ACTION_HOLD);
	}
}

void SFLPhone::on_action_transfer_triggered()
{
	QListWidgetItem * item = listWidget_callList->currentItem();
	if(!item)
	{
		qDebug() << "Error : Transfering when no item selected. Should not happen.";
	}
	else
	{
		action(item, CALL_ACTION_TRANSFER);
	}
}

void SFLPhone::on_action_record_triggered()
{
	QListWidgetItem * item = listWidget_callList->currentItem();
	if(!item)
	{
		qDebug() << "Error : Recording when no item selected. Should not happen.";
	}
	else
	{
		action(item, CALL_ACTION_RECORD);
	}
}

void SFLPhone::on_action_history_triggered(bool checked)
{
	if(checked == true)
	{
		action_addressBook->setChecked(false);
		stackedWidget_screen->setCurrentWidget(page_callHistory);
	}
	else
	{
		stackedWidget_screen->setCurrentWidget(page_callList);
	}
	updateWindowCallState();
}

void SFLPhone::on_action_addressBook_triggered(bool checked)
{
	if(checked == true)
	{
	
		action_history->setChecked(false);
		stackedWidget_screen->setCurrentWidget(page_addressBook);
	}
	else
	{
		stackedWidget_screen->setCurrentWidget(page_callList);
	}
	updateWindowCallState();
}

void SFLPhone::on_action_mailBox_triggered()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	QString account = firstAccountId();
	if(account.isEmpty())
	{
		errorWindow->showMessage("No account registered!");
	}
	else
	{
		QString mailBoxNumber = configurationManager.getAccountDetails(account).value()[ACCOUNT_MAILBOX];
		Call * call = callList->addDialingCall();
		call->appendItemText(mailBoxNumber);
		addCallToCallList(call);
		listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
		actionb(call, CALL_ACTION_ACCEPT);
	}
}

void SFLPhone::on1_callStateChanged(const QString &callID, const QString &state)
{
	qDebug() << "Signal : Call State Changed for call  " << callID << " . New state : " << state;
	Call * call = (*callList)[callID];
	if(!call)
	{
		if(state == CALL_STATE_CHANGE_RINGING)
		{
			call = callList->addRingingCall(callID);
			addCallToCallList(call);
		}
		else
		{
			qDebug() << "Call doesn't exist in this client. Might have been initialized by another client instance before this one started.";
			return;
		}
	}
	else
	{
		call->stateChanged(state);
	}
	updateCallItem(call);
	updateWindowCallState();
}

void SFLPhone::on1_error(MapStringString details)
{
	qDebug() << "Signal : Daemon error : " << details;
}

void SFLPhone::on1_incomingCall(const QString &accountID, const QString & callID, const QString &from)
{
	qDebug() << "Signal : Incoming Call !";
	Call * call = callList->addIncomingCall(callID, from, accountID);
	addCallToCallList(call);
	listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
}

void SFLPhone::on1_incomingMessage(const QString &accountID, const QString &message)
{
	qDebug() << "Signal : Incoming Message ! ";
}

void SFLPhone::on1_voiceMailNotify(const QString &accountID, int count)
{
	qDebug() << "Signal : VoiceMail Notify ! " << count << " new voice mails for account " << accountID;
}

void SFLPhone::on1_volumeChanged(const QString &device, double value)
{
	qDebug() << "Signal : Volume Changed !";
	if(! (toolButton_recVol->isChecked() && value == 0.0))
		updateRecordBar();
	if(! (toolButton_sndVol->isChecked() && value == 0.0))
		updateVolumeBar();
}


