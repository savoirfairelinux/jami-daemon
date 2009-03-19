#include "SFLPhone.h"
#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"
#include "callmanager_interface_singleton.h"
#include <stdlib.h>

SFLPhone::SFLPhone(QMainWindow *parent) : QMainWindow(parent)
{
	setupUi(this);
	
	errorWindow = new QErrorMessage(this);
	callList = new CallList();
	configDialog = new ConfigurationDialog(this);
	configDialog->setModal(true);
	
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
}

void SFLPhone::loadWindow()
{
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	actionAfficher_les_barres_de_volume->setChecked(daemon.getVolumeControls());
	actionAfficher_le_clavier->setChecked(daemon.getDialpad());
	updateWindowCallState();
	updateRecordButton();
	updateVolumeButton();
	updateRecordBar();
	updateVolumeBar();
	updateVolumeControls();
	updateDialpad();
	updateSearchHistory();
}

QString SFLPhone::firstAccount()
{
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	//ask for the list of accounts ids to the daemon
	QStringList accountIds = daemon.getAccountList().value();
	for (int i = 0; i < accountIds.size(); ++i){
		MapStringString accountDetails = daemon.getAccountDetails(accountIds[i]);
		if(accountDetails[QString(ACCOUNT_STATUS)] == QString(ACCOUNT_STATE_REGISTERED))
		{
			return accountIds[i];
		}
	}
	return QString();
}

/*
void SFLPhone::typeChar(QChar c)
{
	QListWidgetItem * item = listWidget_callList->currentItem();
	if(!item)
	{
		qDebug() << "Typing when no item is selected. Opening an item.";
		item = callList->addDialingCall();
		listWidget_callList->addItem(item);
		listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
	}
	listWidget_callList->currentItem()->setText(listWidget_callList->currentItem()->text() + c);
}*/

void SFLPhone::typeString(QString str)
{
	qDebug() << "typeString";
	if(stackedWidget_screen->currentWidget() == page_callList)
	{
		QListWidgetItem * item = listWidget_callList->currentItem();
		if(!item)
		{
			qDebug() << "Typing when no item is selected. Opening an item.";
			item = callList->addDialingCall();
			listWidget_callList->addItem(item);
			listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
		}
		listWidget_callList->currentItem()->setText(listWidget_callList->currentItem()->text() + str);
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		qDebug() << "In call history.";
		label_searchHistory->setText(label_searchHistory->text() + str);
		
	}
}

void SFLPhone::action(QListWidgetItem * item, call_action action)
{
	try
	{
		(*callList)[item]->actionPerformed(action, item->text());
	}
	catch(const char * msg)
	{
		errorWindow->showMessage(QString(msg));
	}
	updateWindowCallState();
}


/*******************************************
******** Update Display Functions **********
*******************************************/



void SFLPhone::updateWindowCallState()
{
	qDebug() << "updateWindowCallState";
	QListWidgetItem * item;
	
	bool enabledActions[6]= {true,true,true,true,true,true};
	char * iconFile;
	char * buttonIconFiles[3] = {ICON_CALL, ICON_HANGUP, ICON_HOLD};
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
			CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
			Call * call = (*callList)[item];
			call_state state = call->getState();
			//qDebug() << "calling getIsRecording on " << call->getCallId();
			//recordActivated = callManager.getIsRecording(call->getCallId());
			recordActivated = call->getRecording();
			switch (state)
			{
				case CALL_STATE_INCOMING:
					qDebug() << "Reached CALL_STATE_INCOMING with call " << (*callList)[item]->getCallId() << ". Updating window.";
					iconFile = ICON_INCOMING;
					buttonIconFiles[0] = ICON_ACCEPT;
					buttonIconFiles[1] = ICON_REFUSE;
					break;
				case CALL_STATE_RINGING:
					qDebug() << "Reached CALL_STATE_RINGING with call " << (*callList)[item]->getCallId() << ". Updating window.";
					enabledActions[2] = false;
					enabledActions[3] = false;
					iconFile = ICON_RINGING;
					break;
				case CALL_STATE_CURRENT:
					qDebug() << "Reached CALL_STATE_CURRENT with call " << (*callList)[item]->getCallId() << ". Updating window.";
					iconFile = ICON_CURRENT;
					recordEnabled = true;
					break;
				case CALL_STATE_DIALING:
					qDebug() << "Reached CALL_STATE_DIALING with call " << (*callList)[item]->getCallId() << ". Updating window.";
					enabledActions[2] = false;
					enabledActions[3] = false;
					enabledActions[4] = false;
					iconFile = ICON_DIALING;
					buttonIconFiles[0] = ICON_ACCEPT;
					break;
				case CALL_STATE_HOLD:
					qDebug() << "Reached CALL_STATE_HOLD with call " << (*callList)[item]->getCallId() << ". Updating window.";
					iconFile = ICON_HOLD;
					buttonIconFiles[2] = ICON_UNHOLD;
					break;		
				case CALL_STATE_FAILURE:
					qDebug() << "Reached CALL_STATE_FAILURE with call " << (*callList)[item]->getCallId() << ". Updating window.";
					enabledActions[0] = false;
					enabledActions[2] = false;
					enabledActions[3] = false;
					enabledActions[4] = false;
					iconFile = ICON_FAILURE;
					break;
				case CALL_STATE_BUSY:
					qDebug() << "Reached CALL_STATE_BUSY with call " << (*callList)[item]->getCallId() << ". Updating window.";
					enabledActions[0] = false;
					enabledActions[2] = false;
					enabledActions[3] = false;
					enabledActions[4] = false;
					iconFile = ICON_BUSY;
				break;
				case CALL_STATE_TRANSFER:
					qDebug() << "Reached CALL_STATE_TRANSFER with call " << (*callList)[item]->getCallId() << ". Updating window.";
					iconFile = ICON_TRANSFER;
					buttonIconFiles[0] = ICON_EXEC_TRANSF;
					transfer = true;
					recordEnabled = true;
					break;
				case CALL_STATE_TRANSF_HOLD:
					qDebug() << "Reached CALL_STATE_TRANSF_HOLD with call " << (*callList)[item]->getCallId() << ". Updating window.";
					iconFile = ICON_TRANSF_HOLD;
					buttonIconFiles[0] = ICON_EXEC_TRANSF;
					buttonIconFiles[2] = ICON_UNHOLD;
					transfer = true;
					break;
				case CALL_STATE_OVER:
					qDebug() << "Reached CALL_STATE_OVER. Deleting item " << (*callList)[item]->getCallId();
					listWidget_callList->takeItem(listWidget_callList->row(item));
					listWidget_callHistory->addItem(call->getHistoryItem());
					listWidget_callHistory->setCurrentRow(listWidget_callHistory->count() - 1);
					return;
					break;
				case CALL_STATE_ERROR:
					qDebug() << "Reached CALL_STATE_ERROR with call " << (*callList)[item]->getCallId() << "!";
					break;
				default:
					qDebug() << "Reached unexisting state for call " << (*callList)[item]->getCallId() << "!";
					break;
			}
		}
		qDebug() << "mi";
		if (item)
		{
			qDebug() << "rentre " << item;
			item->setIcon(QIcon(iconFile));
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
		if(!label_searchHistory->text().isEmpty())
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
	label_searchHistory->setVisible(!label_searchHistory->text().isEmpty());
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
	QString textSearched = label_searchHistory->text();
	for(int i = 0 ; i < callList->size() ; i++)
	{
		Call * call = (*callList)[i];
		qDebug() << "" << call->getCallId();
		if(call->getState() == CALL_STATE_OVER && call->getHistoryItem()->text().contains(textSearched))
		{
			qDebug() << "call->getItem()->text()=" << call->getHistoryItem()->text() << " contains textSearched=" << textSearched;
			listWidget_callHistory->addItem(call->getHistoryItem());
		}
	}
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
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	int display = daemon.getVolumeControls();
	widget_recVol->setVisible(display);
	widget_sndVol->setVisible(display);
}

void SFLPhone::updateDialpad()
{
	qDebug() << "updateDialpad";
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	int display = daemon.getDialpad();
	widget_dialpad->setVisible(display);
}



/************************************************************
************            Autoconnect             *************
************************************************************/

void SFLPhone::on_actionAfficher_les_barres_de_volume_toggled()
{
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	daemon.setVolumeControls();
	updateVolumeControls();
}

void SFLPhone::on_actionAfficher_le_clavier_toggled()
{
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	daemon.setDialpad();
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

void SFLPhone::on_label_searchHistory_textChanged(const QString & text)
{
	qDebug() << "on_label_searchHistory_textEdited";
	updateSearchHistory();
	updateCallHistory();
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

void SFLPhone::on_toolButton_recVol_clicked()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "on_toolButton_recVol_clicked().";
	if(! toolButton_recVol->isChecked())
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
	/*
	qDebug() << "on_toolButton_recVol_clicked(). checked = " << toolButton_recVol->isChecked();
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	toolButton_recVol->setChecked(toolButton_recVol->isChecked());
	//toolButton_recVol->setChecked(true);
	slider_recVol->setEnabled(! toolButton_recVol->isChecked());
	callManager.setVolume(RECORD_DEVICE, toolButton_recVol->isChecked() ? (double)slider_recVol->value() / 100.0 : 0.0);
	updateRecordButton();
	*/
}

void SFLPhone::on_toolButton_sndVol_clicked()
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "on_toolButton_sndVol_clicked().";
	if(! toolButton_sndVol->isChecked())
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
	/*
	qDebug() << "on_toolButton_sndVol_clicked(). checked = " << toolButton_recVol->isChecked();
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	toolButton_sndVol->setChecked(toolButton_sndVol->isChecked());
	slider_sndVol->setEnabled(! toolButton_sndVol->isChecked());
	//callManager.setVolume(SOUND_DEVICE, toolButton_recVol->isChecked() ? 0.0 : (double)slider_sndVol->value() / 100.0);
	callManager.setVolume(SOUND_DEVICE, 0.0);
	updateVolumeButton();
	*/
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

void SFLPhone::on_listWidget_callHistory_currentItemChanged()
{
	qDebug() << "on_listWidget_callHistory_currentItemChanged";
	updateWindowCallState();
}

void SFLPhone::on_actionConfigurer_les_comptes_triggered()
{
	configDialog->loadOptions();
	configDialog->setPage(PAGE_ACCOUNTS);
	configDialog->show();
}

void SFLPhone::on_actionConfigurer_le_son_triggered()
{
	configDialog->loadOptions();
	configDialog->setPage(PAGE_AUDIO);
	configDialog->show();
}

void SFLPhone::on_actionConfigurer_SFLPhone_triggered()
{
	configDialog->loadOptions();
	configDialog->setPage(PAGE_GENERAL);
	configDialog->show();
}

void SFLPhone::on_action_accept_triggered()
{
	if(stackedWidget_screen->currentWidget() == page_callList)
	{
		QListWidgetItem * item = listWidget_callList->currentItem();
		if(!item || (*callList)[item]->getState() == CALL_STATE_RINGING || (*callList)[item]->getState() == CALL_STATE_CURRENT || (*callList)[item]->getState() == CALL_STATE_HOLD || (*callList)[item]->getState() == CALL_STATE_BUSY)
		{
			qDebug() << "Calling when no item is selected or item currently ringing, current, hold or busy. Opening an item.";
			item = callList->addDialingCall();
			listWidget_callList->addItem(item);
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
		QListWidgetItem * item = callList->addDialingCall();
		item->setText(listWidget_callHistory->currentItem()->text());
		listWidget_callList->addItem(item);
		listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
		action(item, CALL_ACTION_ACCEPT);
	}
}

void SFLPhone::on_action_refuse_triggered()
{
	if(stackedWidget_screen->currentWidget() == page_callList)
	{
		QListWidgetItem * item = listWidget_callList->currentItem();
		if(!item)
		{
			qDebug() << "Hanging up when no item selected. Should not happen.";
		}
		else
		{
			action(item, CALL_ACTION_REFUSE);
		}
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		label_searchHistory->clear();
	}
}

void SFLPhone::on_action_hold_triggered()
{
	QListWidgetItem * item = listWidget_callList->currentItem();
	if(!item)
	{
		qDebug() << "Holding when no item selected. Should not happen.";
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
		qDebug() << "Transfering when no item selected. Should not happen.";
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
		qDebug() << "Recording when no item selected. Should not happen.";
	}
	else
	{
		action(item, CALL_ACTION_RECORD);
	}
}

void SFLPhone::on_action_history_toggled(bool checked)
{
	stackedWidget_screen->setCurrentWidget(checked ? page_callHistory : page_callList);
	updateWindowCallState();
}

void SFLPhone::on_action_mailBox_triggered()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	QString account = firstAccount();
	if(account.isEmpty())
	{
		errorWindow->showMessage("No account registered!");
	}
	else
	{
		QString mailBoxNumber = configurationManager.getAccountDetails(account).value()[ACCOUNT_MAILBOX];
		QListWidgetItem * item = callList->addDialingCall();
		item->setText(mailBoxNumber);
		listWidget_callList->addItem(item);
		listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
		action(item, CALL_ACTION_ACCEPT);
	}
}

void SFLPhone::on1_callStateChanged(const QString &callID, const QString &state)
{
	qDebug() << "on_callStateChanged " << callID << " . New state : " << state;
	Call * call = (*callList)[callID];
	if(!call)
	{
		qDebug() << "Call doesn't exist in this client. Might have been initialized by another client instance before this one started.";
	}
	else
	{
		call->stateChanged(state);
	}
	updateWindowCallState();
}

void SFLPhone::on1_error(MapStringString details)
{
	qDebug() << "Daemon error : " << details;
}

void SFLPhone::on1_incomingCall(const QString &accountID, const QString & callID, const QString &from)
{
	qDebug() << "Incoming Call !";
	QListWidgetItem * item = callList->addIncomingCall(callID, from, accountID);
	listWidget_callList->addItem(item);
	listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
}

void SFLPhone::on1_incomingMessage(const QString &accountID, const QString &message)
{
	qDebug() << "on_incomingMessage !";
}

void SFLPhone::on1_voiceMailNotify(const QString &accountID, int count)
{
	qDebug() << "on_voiceMailNotify !";
}

void SFLPhone::on1_volumeChanged(const QString &device, double value)
{
	qDebug() << "on_volumeChanged !";
	if(! (toolButton_recVol->isChecked() && value == 0.0))
		updateRecordBar();
	if(! (toolButton_sndVol->isChecked() && value == 0.0))
		updateVolumeBar();
}


/*void SFLPhone::on_actionAbout()
{

}*/




