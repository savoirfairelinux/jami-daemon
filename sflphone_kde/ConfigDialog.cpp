#include <QtGui>
#include <QtCore>
#include <iostream>
#include <stdarg.h>
#include "sflphone_const.h"
#include "metatypes.h"
#include "ConfigDialog.h"
#include "configurationmanager_interface_singleton.h"

using namespace std;


ConfigurationDialog::ConfigurationDialog(SFLPhone *parent) : QDialog(parent)
{
	//configuration qt designer
	setupUi(this);
	
	//configuration complémentaire
	QStyle * style = QApplication::style();
	errorWindow = new QErrorMessage(this);
	codecPayloads = new MapStringString();
	horizontalSlider_historyCapacity->setMaximum(MAX_HISTORY_CAPACITY);
	label_WarningSIP->setVisible(false);
	for(int i = 0 ; i < list_options->count() ; i++)
	{
		list_options->item(i)->setTextAlignment(Qt::AlignHCenter);
	}
	button_accountUp->setIcon(style->standardIcon(QStyle::SP_ArrowUp));
	button_accountDown->setIcon(style->standardIcon(QStyle::SP_ArrowDown));
	toolButton_codecUp->setIcon(style->standardIcon(QStyle::SP_ArrowUp));
	toolButton_codecDown->setIcon(style->standardIcon(QStyle::SP_ArrowDown));

	//TODO ajouter les items de l'interface audio ici avec les constantes
	
	//configuration dbus
	registerCommTypes();

	
	
	
	
	loadOptions();
}

ConfigurationDialog::~ConfigurationDialog()
{
	delete accountList;
	delete errorWindow;
	delete codecPayloads;
}

void ConfigurationDialog::loadOptions()
{
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	
	////////////////////////
	////General settings////
	////////////////////////
	
	//Call history settings
	spinBox_historyCapacity->setValue(daemon.getMaxCalls());
	
	//SIP port settings
	int sipPort = daemon.getSipPort();
	if(sipPort<1025){
		spinBox_SIPPort->setMinimum(sipPort);
		label_WarningSIP->setText("Attention : le port SIP doit être supérieur à 1024 !");
		label_WarningSIP->setVisible(true);
	}
	if(sipPort>65535){
		spinBox_SIPPort->setMaximum(sipPort);
		label_WarningSIP->setText("Attention : le port SIP doit être inférieur à 65536 !");
		label_WarningSIP->setVisible(true);
	}
	spinBox_SIPPort->setValue(daemon.getSipPort());
	
	////////////////////////
	////Display settings////
	////////////////////////

	//Notification settings
	checkBox1_notifOnCalls->setCheckState(daemon.getNotify() ? Qt::Checked : Qt::Unchecked);
	checkBox2_notifOnMessages->setCheckState(daemon.getMailNotify() ? Qt::Checked : Qt::Unchecked);
	
	//Window display settings
	checkBox1_displayOnStart->setCheckState(daemon.isStartHidden() ? Qt::Unchecked : Qt::Checked);
	checkBox2_displayOnCalls->setCheckState(daemon.popupMode() ? Qt::Checked : Qt::Unchecked);
	
	/////////////////////////
	////Accounts settings////
	/////////////////////////
	
	loadAccountList();

	//Stun settings
	checkBox_stun->setCheckState(daemon.isStunEnabled() ? Qt::Checked : Qt::Unchecked);
	lineEdit_stun->setText(QString(daemon.getStunServer()));
	
	//////////////////////
	////Audio settings////
	//////////////////////
	
	//Audio Interface settings
	comboBox_interface->setCurrentIndex(daemon.getAudioManager());
	stackedWidget_interfaceSpecificSettings->setCurrentIndex(daemon.getAudioManager());
	
	//ringtones settings
	checkBox_ringtones->setCheckState(daemon.isRingtoneEnabled() ? Qt::Checked : Qt::Unchecked);
	//TODO widget choix de sonnerie
	//widget_nomSonnerie->setText(daemon.getRingtoneChoice());
	
	//codecs settings
	loadCodecs();

	//
	//alsa settings
	comboBox1_alsaPlugin->clear();
	QStringList pluginList = daemon.getOutputAudioPluginList();
	comboBox1_alsaPlugin->addItems(pluginList);
	comboBox1_alsaPlugin->setCurrentIndex(comboBox1_alsaPlugin->findText(daemon.getCurrentAudioOutputPlugin()));
	
	qDebug() << "avant daemon.getCurrentAudioDevicesIndex();";
	QStringList devices = daemon.getCurrentAudioDevicesIndex();
	qDebug() << "apres daemon.getCurrentAudioDevicesIndex();";

	int inputDevice = devices[1].toInt();
	comboBox2_in->clear();
	QStringList inputDeviceList = daemon.getAudioInputDeviceList();
	comboBox2_in->addItems(inputDeviceList);
	comboBox2_in->setCurrentIndex(inputDevice);
	
	int outputDevice = devices[0].toInt();
	comboBox3_out->clear();
	QStringList outputDeviceList = daemon.getAudioOutputDeviceList();
	comboBox3_out->addItems(inputDeviceList);
	comboBox3_out->setCurrentIndex(outputDevice);
	
	//pulseaudio settings
	checkBox_pulseAudioVolumeAlter->setCheckState(daemon.getPulseAppVolumeControl() ? Qt::Checked : Qt::Unchecked);
	
	
}


void ConfigurationDialog::saveOptions()
{
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	
	////////////////////////
	////General settings////
	////////////////////////
	
	//Call history settings
	daemon.setMaxCalls(spinBox_historyCapacity->value());
	
	//SIP port settings
	int sipPort = spinBox_SIPPort->value();
	
	if(sipPort<1025){
		errorWindow->showMessage("Attention : le port SIP doit être supérieur à 1024 !");
	}
	if(sipPort>65535){
		errorWindow->showMessage("Attention : le port SIP doit être inférieur à 65536 !");
	}
	daemon.setSipPort(sipPort);
	
	////////////////////////
	////Display settings////
	////////////////////////

	//Notification settings
	if(checkBox1_notifOnCalls->checkState() != (daemon.getNotify() ? Qt::Checked : Qt::Unchecked)) daemon.setNotify();
	if(checkBox2_notifOnMessages->checkState() != (daemon.getMailNotify() ? Qt::Checked : Qt::Unchecked)) daemon.setMailNotify();
	
	//Window display settings
	//WARNING états inversés
	if(checkBox1_displayOnStart->checkState() != (daemon.isStartHidden() ? Qt::Unchecked : Qt::Checked)) daemon.startHidden();
	if(checkBox2_displayOnCalls->checkState() != (daemon.popupMode() ? Qt::Checked : Qt::Unchecked)) daemon.switchPopupMode();
	
	/////////////////////////
	////Accounts settings////
	/////////////////////////
	
	saveAccountList();

	//Stun settings
	if(checkBox_stun->checkState() != (daemon.isStunEnabled() ? Qt::Checked : Qt::Unchecked)) daemon.enableStun();
	daemon.setStunServer(lineEdit_stun->text());

	//////////////////////
	////Audio settings////
	//////////////////////
	
	//Audio Interface settings
	qDebug() << "setting audio manager";
	int manager = comboBox_interface->currentIndex();
	daemon.setAudioManager(manager);
	
	//ringtones settings
	qDebug() << "setting ringtone options";
	if(checkBox_ringtones->checkState() != (daemon.isRingtoneEnabled() ? Qt::Checked : Qt::Unchecked)) daemon.ringtoneEnabled();
	//TODO widget choix de sonnerie
	//daemon.getRingtoneChoice(widget_nomSonnerie->text());
	
	//codecs settings
	qDebug() << "saving codecs";
	saveCodecs();

	//alsa settings
	if(manager == ALSA)
	{
		qDebug() << "setting alsa settings";
		daemon.setOutputAudioPlugin(comboBox1_alsaPlugin->currentText());
		daemon.setAudioInputDevice(comboBox2_in->currentIndex());
		daemon.setAudioOutputDevice(comboBox3_out->currentIndex());
	}
	//pulseaudio settings
	if(manager == PULSEAUDIO)
	{
		qDebug() << "setting pulseaudio settings";
		if(checkBox_pulseAudioVolumeAlter->checkState() != (daemon.getPulseAppVolumeControl() ? Qt::Checked : Qt::Unchecked)) daemon.setPulseAppVolumeControl();
	}
}


void ConfigurationDialog::loadAccountList()
{
	//ask for the list of accounts ids to the daemon
	QStringList accountIds = ConfigurationManagerInterfaceSingleton::getInstance().getAccountList().value();
	//create the AccountList object with the ids
	accountList = new AccountList(accountIds);
	//initialize the QListWidget object with the AccountList
	listWidget_accountList->clear();
	for (int i = 0; i < accountList->size(); ++i){
		listWidget_accountList->addItem((*accountList)[i].getItem());
	}
	if (listWidget_accountList->count() > 0) 
		listWidget_accountList->setCurrentRow(0);
	else 
		frame2_editAccounts->setEnabled(false);
}

void ConfigurationDialog::saveAccountList()
{
	//save the account being edited
	if(listWidget_accountList->currentItem())
		saveAccount(listWidget_accountList->currentItem());
	//get the daemon instance
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	//ask for the list of accounts ids to the daemon
	QStringList accountIds= QStringList(daemon.getAccountList().value());
	//create or update each account from accountList
	for (int i = 0; i < accountList->size(); i++){
		Account & current = (*accountList)[i];
		QString currentId;
		//if the account has no instanciated id, it has just been created in the client
		if(current.isNew())
		{
			currentId = QString(daemon.addAccount(current.getAccountDetails()));
		}
		//if the account has an instanciated id but it's not in daemon
		else{
			if(! accountIds.contains(current.getAccountId()))
			{
				qDebug() << "The account with id " << current.getAccountId() << " doesn't exist. It might have been removed by another SFLPhone client.";
				currentId = QString("");
			}
			else
			{
				daemon.setAccountDetails(current.getAccountId(), current.getAccountDetails());
				currentId = QString(current.getAccountId());
			}
		}
		daemon.sendRegister(currentId, (current.getItem()->checkState() == Qt::Checked) ? 1 : 0  );
	}
	//remove accounts that are in the daemon but not in the client
	for (int i = 0; i < accountIds.size(); i++)
		if(! accountList->getAccountById(accountIds[i])){
			qDebug() << "remove account " << accountIds[i];
			daemon.removeAccount(accountIds[i]);
		}
}

void ConfigurationDialog::loadAccount(QListWidgetItem * item)
{
	if(! item )  {  qDebug() << "Attempting to load details of an account from a NULL item";  return;  }

	Account * account = accountList->getAccountByItem(item);
	if(! account )  {  qDebug() << "Attempting to load details of an unexisting account";  return;  }

	edit1_alias->setText( account->getAccountDetail(*(new QString(ACCOUNT_ALIAS))));
	int protocoleIndex = getProtocoleIndex(account->getAccountDetail(*(new QString(ACCOUNT_TYPE))));
	edit2_protocol->setCurrentIndex( (protocoleIndex < 0) ? 0 : protocoleIndex );
	edit3_server->setText( account->getAccountDetail(*(new QString(ACCOUNT_HOSTNAME))));
	edit4_user->setText( account->getAccountDetail(*(new QString(ACCOUNT_USERNAME))));
	edit5_password->setText( account->getAccountDetail(*(new QString(ACCOUNT_PASSWORD))));
	edit6_mailbox->setText( account->getAccountDetail(*(new QString(ACCOUNT_MAILBOX))));
	QString status = account->getAccountDetail(*(new QString(ACCOUNT_STATUS)));
	edit7_state->setText( "<FONT COLOR=\"" + account->getStateColorName() + "\">" + status + "</FONT>" );
	//edit7_Etat->setTextColor( account->getStateColor );
}


void ConfigurationDialog::saveAccount(QListWidgetItem * item)
{
	if(! item)  { qDebug() << "Attempting to save details of an account from a NULL item"; return; }
	
	Account * account = accountList->getAccountByItem(item);
	if(! account)  {  qDebug() << "Attempting to save details of an unexisting account : " << item->text(); return;  }

	account->setAccountDetail(ACCOUNT_ALIAS, edit1_alias->text());
	account->setAccountDetail(ACCOUNT_TYPE, getIndexProtocole(edit2_protocol->currentIndex()));
	account->setAccountDetail(ACCOUNT_HOSTNAME, edit3_server->text());
	account->setAccountDetail(ACCOUNT_USERNAME, edit4_user->text());
	account->setAccountDetail(ACCOUNT_PASSWORD, edit5_password->text());
	account->setAccountDetail(ACCOUNT_MAILBOX, edit6_mailbox->text());
	
}


void ConfigurationDialog::loadCodecs()
{
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	QStringList codecList = daemon.getCodecList();
	QStringList activeCodecList = daemon.getActiveCodecList();
	qDebug() << codecList;
	qDebug() << activeCodecList;
	tableWidget_codecs->setRowCount(0);
	codecPayloads->clear();
	for(int i=0 ; i<codecList.size() ; i++)
	{
		bool ok;
		qDebug() << codecList[i];
		QString payloadStr = QString(codecList[i]);
		int payload = payloadStr.toInt(&ok);
		if(!ok)	
			qDebug() << "The codec's payload sent by the daemon is not a number : " << codecList[i];
		else
		{
			QStringList details = daemon.getCodecDetails(payload);
			tableWidget_codecs->insertRow(i);
			QTableWidgetItem * headerItem = new QTableWidgetItem("");
			tableWidget_codecs->setVerticalHeaderItem (i, headerItem);
			//headerItem->setVisible(false);
			tableWidget_codecs->setItem(i,0,new QTableWidgetItem(""));
			tableWidget_codecs->setItem(i,1,new QTableWidgetItem(details[CODEC_NAME]));
			//qDebug() << "tableWidget_Codecs->itemAt(" << i << "," << 2 << ")->setText(" << details[CODEC_NAME] << ");";
			//tableWidget_Codecs->item(i,2)->setText(details[CODEC_NAME]);
			tableWidget_codecs->setItem(i,2,new QTableWidgetItem(details[CODEC_SAMPLE_RATE]));
			tableWidget_codecs->setItem(i,3,new QTableWidgetItem(details[CODEC_BIT_RATE]));
			tableWidget_codecs->setItem(i,4,new QTableWidgetItem(details[CODEC_BANDWIDTH]));
			tableWidget_codecs->item(i,0)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
			tableWidget_codecs->item(i,0)->setCheckState(activeCodecList.contains(codecList[i]) ? Qt::Checked : Qt::Unchecked);
			tableWidget_codecs->item(i,1)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
			tableWidget_codecs->item(i,2)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
			tableWidget_codecs->item(i,3)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
			tableWidget_codecs->item(i,4)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
			(*codecPayloads)[details[CODEC_NAME]] = payloadStr;
			qDebug() << "Added to codecs : " << payloadStr << " , " << details[CODEC_NAME];
		}
	}
	tableWidget_codecs->resizeColumnsToContents();
	tableWidget_codecs->resizeRowsToContents();
}


void ConfigurationDialog::saveCodecs()
{
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	QStringList activeCodecs;
	for(int i = 0 ; i < tableWidget_codecs->rowCount() ; i++)
	{
		//qDebug() << "Checked if active : " << tableWidget_Codecs->item(i,1)->text();
		if(tableWidget_codecs->item(i,0)->checkState() == Qt::Checked)
		{
			//qDebug() << "Added to activeCodecs : " << tableWidget_Codecs->item(i,1)->text();
			activeCodecs << (*codecPayloads)[tableWidget_codecs->item(i,1)->text()];
		}
	}
	qDebug() << "Calling setActiveCodecList with list : " << activeCodecs ;
	daemon.setActiveCodecList(activeCodecs);
}

void ConfigurationDialog::setPage(int page)
{
	stackedWidget_options->setCurrentIndex(page);
	list_options->setCurrentRow(page);
}

void ConfigurationDialog::updateAccountListCommands()
{
	bool buttonsEnabled[4] = {true,true,true,true};
	if(! listWidget_accountList->currentItem())
	{
		buttonsEnabled[0] = false;
		buttonsEnabled[1] = false;
		buttonsEnabled[3] = false;
	}
	else if(listWidget_accountList->currentRow() == 0)
	{
		buttonsEnabled[0] = false;
	}
	else if(listWidget_accountList->currentRow() == listWidget_accountList->count() - 1)
	{
		buttonsEnabled[1] = false;
	}
	button_accountUp->setEnabled(buttonsEnabled[0]);
	button_accountDown->setEnabled(buttonsEnabled[1]);
	button_accountAdd->setEnabled(buttonsEnabled[2]);
	button_accountRemove->setEnabled(buttonsEnabled[3]);
}

void ConfigurationDialog::updateCodecListCommands()
{
	bool buttonsEnabled[2] = {true,true};
	if(! listWidget_accountList->currentItem())
	{
		buttonsEnabled[0] = false;
		buttonsEnabled[1] = false;
	}
	else if(listWidget_accountList->currentRow() == 0)
	{
		buttonsEnabled[0] = false;
	}
	else if(listWidget_accountList->currentRow() == listWidget_accountList->count() - 1)
	{
		buttonsEnabled[1] = false;
	}
	toolButton_codecUp->setEnabled(buttonsEnabled[0]);
	toolButton_codecDown->setEnabled(buttonsEnabled[1]);
}

void ConfigurationDialog::on_edit1_alias_textChanged(const QString & text)
{
	listWidget_accountList->currentItem()->setText(text);
}

void ConfigurationDialog::on_spinBox_SIPPort_valueChanged ( int value )
{
	if(value>1024 && value<65536)
		label_WarningSIP->setVisible(false);
	else
		label_WarningSIP->setVisible(true);
}

void ConfigurationDialog::on_listWidget_codecs_currentItemChanged ( QListWidgetItem * current, QListWidgetItem * previous )
{
	qDebug() << "on_listWidget_codecs_currentItemChanged";
	updateCodecListCommands();
}

void ConfigurationDialog::on_toolButton_codecUp_clicked()
{
}

void ConfigurationDialog::on_toolButton_codecDown_clicked()
{/*
	int currentRow = listWidget_codecs->currentRow();
	int nbCol = tableWidget_codecs->columnCount();
	QTableWidgetSelectionRange row(currentRow, 0, currentRow, nbCol - 1);
	QListWidgetItem * item = listWidget_accountList->takeItem(currentRow);
	listWidget_accountList->insertItem(currentRow + 1 , item);
	listWidget_accountList->setCurrentItem(item);
*/}

void ConfigurationDialog::on_listWidget_accountList_currentItemChanged ( QListWidgetItem * current, QListWidgetItem * previous )
{
	qDebug() << "on_listWidget_accountList_currentItemChanged";
	if(previous)
		saveAccount(previous);
	if(current)
		loadAccount(current);
	updateAccountListCommands();
}

void ConfigurationDialog::on_button_accountUp_clicked()
{
	int currentRow = listWidget_accountList->currentRow();
	QListWidgetItem * item = listWidget_accountList->takeItem(currentRow);
	listWidget_accountList->insertItem(currentRow - 1 , item);
	listWidget_accountList->setCurrentItem(item);
}

void ConfigurationDialog::on_button_accountDown_clicked()
{
	int currentRow = listWidget_accountList->currentRow();
	QListWidgetItem * item = listWidget_accountList->takeItem(currentRow);
	listWidget_accountList->insertItem(currentRow + 1 , item);
	listWidget_accountList->setCurrentItem(item);
}

void ConfigurationDialog::on_button_accountAdd_clicked()
{
	QString itemName = QInputDialog::getText(this, "New account", "Enter new account's alias");
	itemName = itemName.simplified();
	if (!itemName.isEmpty()) {
		QListWidgetItem * item = accountList->addAccount(itemName);
     
		//TODO verifier que addItem set bien le parent
		listWidget_accountList->addItem(item);
		int r = listWidget_accountList->count() - 1;
		listWidget_accountList->setCurrentRow(r);
		frame2_editAccounts->setEnabled(true);
	}
}

void ConfigurationDialog::on_button_accountRemove_clicked()
{
	int r = listWidget_accountList->currentRow();
	QListWidgetItem * item = listWidget_accountList->takeItem(r);
	accountList->removeAccount(item);
	listWidget_accountList->setCurrentRow( (r >= listWidget_accountList->count()) ? r-1 : r );
}


void ConfigurationDialog::on_buttonBoxDialog_clicked(QAbstractButton * button)
{
	if(buttonBoxDialog->standardButton(button) == QDialogButtonBox::Apply)
	{
		this->saveOptions();
		this->loadOptions();
	}
	if(buttonBoxDialog->standardButton(button) == QDialogButtonBox::RestoreDefaults)
	{
		this->loadOptions();
	}
	if(buttonBoxDialog->standardButton(button) == QDialogButtonBox::Ok)
	{
		this->saveOptions();
		this->setVisible(false);
	}
	if(buttonBoxDialog->standardButton(button) == QDialogButtonBox::Cancel)
	{
		this->setVisible(false);
	}
}

void ConfigurationDialog::on_tableWidget_codecs_currentItemChanged(QTableWidgetItem * current, QTableWidgetItem * previous)
{
	qDebug() << "on_tableWidget_codecs_currentItemChanged";
	int row = current->row();
	int nbCol = tableWidget_codecs->columnCount();
	for(int i = 0 ; i < nbCol ; i++)
	{
		tableWidget_codecs->setRangeSelected(QTableWidgetSelectionRange(row, 0, row, nbCol - 1), true);
	}
	updateCodecListCommands();
}

void ConfigurationDialog::on_tableWidget_codecs_currentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn)
{
	qDebug() << "on_tableWidget_codecs_currentCellChanged";
	int nbCol = tableWidget_codecs->columnCount();
	for(int i = 0 ; i < nbCol ; i++)
	{
		tableWidget_codecs->setRangeSelected(QTableWidgetSelectionRange(currentRow, 0, currentRow, nbCol - 1), true);
	}
	updateCodecListCommands();
}

/*
void ConfigurationDialog::on_listWidgetComptes_itemChanged(QListWidgetItem * item)
{
	if(! item)  { qDebug() << "Attempting to save details of an account from a NULL item\n"; return; }
	
	Account * account = accountList->getAccountByItem(item);
	if(! account)  {  qDebug() << "Attempting to save details of an unexisting account\n"; return;  }

	if(item->checkState() != account->getAccountState)
	
	if(buttonBoxDialog->standardButton(button) == QDialogButtonBox::Apply)
	{
		this->saveOptions();
		this->loadOptions();
	}
	if(buttonBoxDialog->standardButton(button) == QDialogButtonBox::RestoreDefaults)
	{
		this->loadOptions();
	}
	if(buttonBoxDialog->standardButton(button) == QDialogButtonBox::Ok)
	{
		this->saveOptions();
		this->setVisible(false);
	}
	if(buttonBoxDialog->standardButton(button) == QDialogButtonBox::Cancel)
	{
		this->setVisible(false);
	}
}
*/