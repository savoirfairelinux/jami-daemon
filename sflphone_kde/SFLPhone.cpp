#include "SFLPhone.h"
#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"
#include "callmanager_interface_singleton.h"
#include <stdlib.h>

SFLPhone::SFLPhone(QMainWindow *parent) : QMainWindow(parent)
{
	setupUi(this);
    
	callList = new CallList();
    
	configDialog = new ConfigurationDialog(this);
	configDialog->setModal(true);
	
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	connect(&callManager, SIGNAL(callStateChanged(const QString &, const QString &)),
	        this,         SLOT(on_callStateChanged(const QString &, const QString &)));
	connect(&callManager, SIGNAL(error(MapStringString)),
	        this,         SLOT(on_error(MapStringString)));
	connect(&callManager, SIGNAL(incomingCall(const QString &, const QString &, const QString &)),
	        this,         SLOT(on_incomingCall(const QString &, const QString &, const QString &)));
	connect(&callManager, SIGNAL(incomingMessage(const QString &, const QString &)),
	        this,         SLOT(on_incomingMessage(const QString &, const QString &)));
	connect(&callManager, SIGNAL(voiceMailNotify(const QString &, int)),
	        this,         SLOT(on_voiceMailNotify(const QString &, int)));
	connect(&callManager, SIGNAL(volumeChanged(const QString &, double)),
	        this,         SLOT(on_volumeChanged(const QString &, double)));
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
	return "";
}

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
}

void SFLPhone::action(QListWidgetItem * item, call_action action)
{
	(*callList)[item]->action(action, item->text());
	updateWindowCallState();
}


/*******************************************
******** Update Display Functions **********
*******************************************/

void SFLPhone::updateWindowCallState()
{
	qDebug() << "updateWindowCallState";
	QListWidgetItem * item = listWidget_callList->currentItem();
	
	bool enabledActions[5]= {true,true,true,true,true};
	char * iconFile;
	char * buttonIconFiles[3] = {ICON_CALL, ICON_HANGUP, ICON_HOLD};
	bool transfer = false;
	bool record = false;
	
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
		call_state state = (*callList)[item]->getState();
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
				break;
			case CALL_STATE_DIALING:
				qDebug() << "Reached CALL_STATE_DIALING with call " << (*callList)[item]->getCallId() << ". Updating window.";
				enabledActions[2] = false;
				enabledActions[3] = false;
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
				break;
			case CALL_STATE_TRANSFER_HOLD:
				qDebug() << "Reached CALL_STATE_TRANSFER_HOLD with call " << (*callList)[item]->getCallId() << ". Updating window.";
				iconFile = ICON_TRANSFER_HOLD;
				buttonIconFiles[0] = ICON_EXEC_TRANSF;
				buttonIconFiles[2] = ICON_UNHOLD;
				transfer = true;
				break;
			case CALL_STATE_OVER:
				qDebug() << "Reached CALL_STATE_OVER. Deleting call " << (*callList)[item]->getCallId();
				//delete (*callList)[item];
				callList->remove((*callList)[item]);
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
		QIcon icon = QIcon(iconFile);
		item->setIcon(icon);
	}
	actionDecrocher->setEnabled(enabledActions[0]);
	actionRaccrocher->setEnabled(enabledActions[1]);
	actionMettre_en_attente->setEnabled(enabledActions[2]);
	actionTransferer->setEnabled(enabledActions[3]);
	actionRecord->setEnabled(enabledActions[4]);
	
	actionDecrocher->setIcon(QIcon(buttonIconFiles[0]));
	actionRaccrocher->setIcon(QIcon(buttonIconFiles[1]));
	actionMettre_en_attente->setIcon(QIcon(buttonIconFiles[2]));
	
	actionTransferer->setChecked(transfer);
	//actionRecord->setChecked(record);
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

void SFLPhone::on_pushButton_1_clicked()      { typeChar('1'); }
void SFLPhone::on_pushButton_2_clicked()      { typeChar('2'); }
void SFLPhone::on_pushButton_3_clicked()      { typeChar('3'); }
void SFLPhone::on_pushButton_4_clicked()      { typeChar('4'); }
void SFLPhone::on_pushButton_5_clicked()      { typeChar('5'); }
void SFLPhone::on_pushButton_6_clicked()      { typeChar('6'); }
void SFLPhone::on_pushButton_7_clicked()      { typeChar('7'); }
void SFLPhone::on_pushButton_8_clicked()      { typeChar('8'); }
void SFLPhone::on_pushButton_9_clicked()      { typeChar('9'); }
void SFLPhone::on_pushButton_0_clicked()      { typeChar('0'); }
void SFLPhone::on_pushButton_diese_clicked()  { typeChar('#'); }
void SFLPhone::on_pushButton_etoile_clicked() { typeChar('*'); }

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

void SFLPhone::on_actionDecrocher_triggered()
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

void SFLPhone::on_actionRaccrocher_triggered()
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

void SFLPhone::on_actionMettre_en_attente_triggered()
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

void SFLPhone::on_actionTransferer_triggered()
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

void SFLPhone::on_actionRecord_triggered()
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

void SFLPhone::on_actionHistorique_triggered()
{

}

void SFLPhone::on_actionBoite_vocale_triggered()
{

}

void SFLPhone::on_callStateChanged(const QString &callID, const QString &state)
{
	qDebug() << "on_callStateChanged " << callID << " . New state : " << state;
	(*callList)[callID]->action(CALL_ACTION_STATE_CHANGED, state);
	updateWindowCallState();
}

void SFLPhone::on_error(MapStringString details)
{
	qDebug() << "Daemon error : " << details;
}

void SFLPhone::on_incomingCall(const QString &accountID, const QString & callID, const QString &from)
{
	qDebug() << "Incoming Call !";
	QListWidgetItem * item = callList->addIncomingCall(callID, from, accountID);
	listWidget_callList->addItem(item);
	listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
}

void SFLPhone::on_incomingMessage(const QString &accountID, const QString &message)
{
	qDebug() << "on_incomingMessage !";
}

void SFLPhone::on_voiceMailNotify(const QString &accountID, int count)
{
	qDebug() << "on_voiceMailNotify !";
}

void SFLPhone::on_volumeChanged(const QString &device, double value)
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




