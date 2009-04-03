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
	tableWidget_codecs->verticalHeader()->hide();
	tableWidget_codecs->setSelectionBehavior(QAbstractItemView::SelectRows);
	//tableWidget_codecs->setStyleSheet("border-style: hidden;");



	//TODO ajouter les items de l'interface audio ici avec les constantes
	
	//configuration dbus
	registerCommTypes();

	
	
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	connect(&configurationManager, SIGNAL(accountsChanged()),
	        this,                  SLOT(on1_accountsChanged()));
	
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
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	
	////////////////////////
	////General settings////
	////////////////////////
	
	//Call history settings
	spinBox_historyCapacity->setValue(configurationManager.getMaxCalls());
	
	//SIP port settings
	int sipPort = configurationManager.getSipPort();
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
	spinBox_SIPPort->setValue(configurationManager.getSipPort());
	
	////////////////////////
	////Display settings////
	////////////////////////

	//Notification settings
	checkBox1_notifOnCalls->setCheckState(configurationManager.getNotify() ? Qt::Checked : Qt::Unchecked);
	checkBox2_notifOnMessages->setCheckState(configurationManager.getMailNotify() ? Qt::Checked : Qt::Unchecked);
	
	//Window display settings
	checkBox1_displayOnStart->setCheckState(configurationManager.isStartHidden() ? Qt::Unchecked : Qt::Checked);
	checkBox2_displayOnCalls->setCheckState(configurationManager.popupMode() ? Qt::Checked : Qt::Unchecked);
	
	/////////////////////////
	////Accounts settings////
	/////////////////////////
	
	loadAccountList();

	//Stun settings
	checkBox_stun->setCheckState(configurationManager.isStunEnabled() ? Qt::Checked : Qt::Unchecked);
	lineEdit_stun->setText(QString(configurationManager.getStunServer()));
	
	//////////////////////
	////Audio settings////
	//////////////////////
	
	//Audio Interface settings
	comboBox_interface->setCurrentIndex(configurationManager.getAudioManager());
	stackedWidget_interfaceSpecificSettings->setCurrentIndex(configurationManager.getAudioManager());
	
	//ringtones settings
	checkBox_ringtones->setCheckState(configurationManager.isRingtoneEnabled() ? Qt::Checked : Qt::Unchecked);
	urlComboRequester_ringtone->setUrl(KUrl::fromPath(configurationManager.getRingtoneChoice()));
	
	//codecs settings
	loadCodecs();

	//
	//alsa settings
	comboBox1_alsaPlugin->clear();
	QStringList pluginList = configurationManager.getOutputAudioPluginList();
	comboBox1_alsaPlugin->addItems(pluginList);
	comboBox1_alsaPlugin->setCurrentIndex(comboBox1_alsaPlugin->findText(configurationManager.getCurrentAudioOutputPlugin()));
	
	QStringList devices = configurationManager.getCurrentAudioDevicesIndex();
	
	int inputDevice = devices[1].toInt();
	comboBox2_in->clear();
	QStringList inputDeviceList = configurationManager.getAudioInputDeviceList();
	comboBox2_in->addItems(inputDeviceList);
	comboBox2_in->setCurrentIndex(inputDevice);
	
	int outputDevice = devices[0].toInt();
	comboBox3_out->clear();
	QStringList outputDeviceList = configurationManager.getAudioOutputDeviceList();
	comboBox3_out->addItems(inputDeviceList);
	comboBox3_out->setCurrentIndex(outputDevice);
	
	//pulseaudio settings
	checkBox_pulseAudioVolumeAlter->setCheckState(configurationManager.getPulseAppVolumeControl() ? Qt::Checked : Qt::Unchecked);
	
	
}


void ConfigurationDialog::saveOptions()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	
	////////////////////////
	////General settings////
	////////////////////////
	
	//Call history settings
	configurationManager.setMaxCalls(spinBox_historyCapacity->value());
	
	//SIP port settings
	int sipPort = spinBox_SIPPort->value();
	
	if(sipPort<1025){
		errorWindow->showMessage("Attention : le port SIP doit être supérieur à 1024 !");
	}
	if(sipPort>65535){
		errorWindow->showMessage("Attention : le port SIP doit être inférieur à 65536 !");
	}
	configurationManager.setSipPort(sipPort);
	
	////////////////////////
	////Display settings////
	////////////////////////

	//Notification settings
	if(checkBox1_notifOnCalls->checkState() != (configurationManager.getNotify() ? Qt::Checked : Qt::Unchecked)) configurationManager.setNotify();
	if(checkBox2_notifOnMessages->checkState() != (configurationManager.getMailNotify() ? Qt::Checked : Qt::Unchecked)) configurationManager.setMailNotify();
	
	//Window display settings
	//WARNING états inversés
	if(checkBox1_displayOnStart->checkState() != (configurationManager.isStartHidden() ? Qt::Unchecked : Qt::Checked)) configurationManager.startHidden();
	if(checkBox2_displayOnCalls->checkState() != (configurationManager.popupMode() ? Qt::Checked : Qt::Unchecked)) configurationManager.switchPopupMode();
	
	/////////////////////////
	////Accounts settings////
	/////////////////////////
	
	saveAccountList();

	//Stun settings
	if(checkBox_stun->checkState() != (configurationManager.isStunEnabled() ? Qt::Checked : Qt::Unchecked)) configurationManager.enableStun();
	configurationManager.setStunServer(lineEdit_stun->text());

	//////////////////////
	////Audio settings////
	//////////////////////
	
	//Audio Interface settings
	qDebug() << "setting audio manager";
	int manager = comboBox_interface->currentIndex();
	configurationManager.setAudioManager(manager);
	
	//ringtones settings
	qDebug() << "setting ringtone options";
	if(checkBox_ringtones->checkState() != (configurationManager.isRingtoneEnabled() ? Qt::Checked : Qt::Unchecked)) configurationManager.ringtoneEnabled();
	configurationManager.setRingtoneChoice(urlComboRequester_ringtone->url().url());
	
	//codecs settings
	qDebug() << "saving codecs";
	saveCodecs();

	//alsa settings
	if(manager == ALSA)
	{
		qDebug() << "setting alsa settings";
		configurationManager.setOutputAudioPlugin(comboBox1_alsaPlugin->currentText());
		configurationManager.setAudioInputDevice(comboBox2_in->currentIndex());
		configurationManager.setAudioOutputDevice(comboBox3_out->currentIndex());
	}
	//pulseaudio settings
	if(manager == PULSEAUDIO)
	{
		qDebug() << "setting pulseaudio settings";
		if(checkBox_pulseAudioVolumeAlter->checkState() != (configurationManager.getPulseAppVolumeControl() ? Qt::Checked : Qt::Unchecked)) configurationManager.setPulseAppVolumeControl();
	}
}


void ConfigurationDialog::loadAccountList()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	//ask for the list of accounts ids to the configurationManager
	QStringList accountIds = configurationManager.getAccountList().value();
	//create the AccountList object with the ids
	accountList = new AccountList(accountIds);
	//initialize the QListWidget object with the AccountList
	listWidget_accountList->clear();
	for (int i = 0; i < accountList->size(); ++i){
		addAccountToAccountList(&(*accountList)[i]);
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
	//get the configurationManager instance
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	//ask for the list of accounts ids to the configurationManager
	QStringList accountIds= QStringList(configurationManager.getAccountList().value());
	//create or update each account from accountList
	for (int i = 0; i < accountList->size(); i++){
		Account & current = (*accountList)[i];
		QString currentId;
		//if the account has no instanciated id, it has just been created in the client
		if(current.isNew())
		{
			currentId = configurationManager.addAccount(current.getAccountDetails());
		}
		//if the account has an instanciated id but it's not in configurationManager
		else{
			if(! accountIds.contains(current.getAccountId()))
			{
				qDebug() << "The account with id " << current.getAccountId() << " doesn't exist. It might have been removed by another SFLPhone client.";
				currentId = QString("");
			}
			else
			{
				configurationManager.setAccountDetails(current.getAccountId(), current.getAccountDetails());
				currentId = QString(current.getAccountId());
			}
		}
		configurationManager.sendRegister(currentId, current.isChecked() ? 1 : 0 );
	}
	//remove accounts that are in the configurationManager but not in the client
	for (int i = 0; i < accountIds.size(); i++)
	{
		if(! accountList->getAccountById(accountIds[i]))
		{
			qDebug() << "remove account " << accountIds[i];
			configurationManager.removeAccount(accountIds[i]);
		}
	}
}

void ConfigurationDialog::loadAccount(QListWidgetItem * item)
{
	if(! item )  {  qDebug() << "Attempting to load details of an account from a NULL item";  return;  }

	Account * account = accountList->getAccountByItem(item);
	if(! account )  {  qDebug() << "Attempting to load details of an unexisting account";  return;  }

	edit1_alias->setText( account->getAccountDetail(*(new QString(ACCOUNT_ALIAS))));
	
	QString protocolsTab[] = ACCOUNT_TYPES_TAB;
	QList<QString> * protocolsList = new QList<QString>();
	for(int i=0;i<sizeof(protocolsTab)/sizeof(QString);i++) protocolsList->append(protocolsTab[i]);
	QString accountName = account->getAccountDetail(* new QString(ACCOUNT_TYPE));
	int protocolIndex = protocolsList->indexOf(accountName);
	delete protocolsList;
	
	edit2_protocol->setCurrentIndex( (protocolIndex < 0) ? 0 : protocolIndex );
	edit3_server->setText( account->getAccountDetail(*(new QString(ACCOUNT_HOSTNAME))));
	edit4_user->setText( account->getAccountDetail(*(new QString(ACCOUNT_USERNAME))));
	edit5_password->setText( account->getAccountDetail(*(new QString(ACCOUNT_PASSWORD))));
	edit6_mailbox->setText( account->getAccountDetail(*(new QString(ACCOUNT_MAILBOX))));
	QString status = account->getAccountDetail(*(new QString(ACCOUNT_STATUS)));
	qDebug() << "Color : " << account->getStateColorName();
	edit7_state->setText( "<FONT COLOR=\"" + account->getStateColorName() + "\">" + status + "</FONT>" );
	//edit7_Etat->setTextColor( account->getStateColor );
}


void ConfigurationDialog::saveAccount(QListWidgetItem * item)
{
	if(! item)  { qDebug() << "Attempting to save details of an account from a NULL item"; return; }
	
	Account * account = accountList->getAccountByItem(item);
	if(! account)  {  qDebug() << "Attempting to save details of an unexisting account : " << item->text(); return;  }

	account->setAccountDetail(ACCOUNT_ALIAS, edit1_alias->text());
	QString protocolsTab[] = ACCOUNT_TYPES_TAB;
	account->setAccountDetail(ACCOUNT_TYPE, protocolsTab[edit2_protocol->currentIndex()]);
	account->setAccountDetail(ACCOUNT_HOSTNAME, edit3_server->text());
	account->setAccountDetail(ACCOUNT_USERNAME, edit4_user->text());
	account->setAccountDetail(ACCOUNT_PASSWORD, edit5_password->text());
	account->setAccountDetail(ACCOUNT_MAILBOX, edit6_mailbox->text());
	//account->setAccountDetail(ACCOUNT_ENABLED, account->getItemWidget()->findChild(*(new QString("checkbox"))).checkState() == Qt::Checked ? ACCOUNT_ENABLED_TRUE : ACCOUNT_ENABLED_FALSE);
	account->setItemText(edit1_alias->text());
}

void ConfigurationDialog::addAccountToAccountList(Account * account)
{
	qDebug() << "addAccountToAccountList";
	QListWidgetItem * item = account->getItem();
	QWidget * widget = account->getItemWidget();
	listWidget_accountList->addItem(item);
	listWidget_accountList->setItemWidget(item, widget);
}

void ConfigurationDialog::loadCodecs()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	QStringList codecList = configurationManager.getCodecList();
	QStringList activeCodecList = configurationManager.getActiveCodecList();
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
			qDebug() << "The codec's payload sent by the configurationManager is not a number : " << codecList[i];
		else
		{
			QStringList details = configurationManager.getCodecDetails(payload);
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
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
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
	configurationManager.setActiveCodecList(activeCodecs);
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
	if(! tableWidget_codecs->currentItem())
	{
		buttonsEnabled[0] = false;
		buttonsEnabled[1] = false;
	}
	else if(tableWidget_codecs->currentRow() == 0)
	{
		buttonsEnabled[0] = false;
	}
	else if(tableWidget_codecs->currentRow() == tableWidget_codecs->rowCount() - 1)
	{
		buttonsEnabled[1] = false;
	}
	toolButton_codecUp->setEnabled(buttonsEnabled[0]);
	toolButton_codecDown->setEnabled(buttonsEnabled[1]);
}

void ConfigurationDialog::on_edit1_alias_textChanged(const QString & text)
{
	qDebug() << "on_edit1_alias_textChanged";
	//listWidget_accountList->currentItem()->setText(text);
}

void ConfigurationDialog::on_spinBox_SIPPort_valueChanged ( int value )
{
	qDebug() << "on_spinBox_SIPPort_valueChanged";
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
	qDebug() << "on_toolButton_codecUp_clicked";
	int currentCol = tableWidget_codecs->currentColumn();
	int currentRow = tableWidget_codecs->currentRow();
	int nbCol = tableWidget_codecs->columnCount();
	for(int i = 0 ; i < nbCol ; i++)
	{
		QTableWidgetItem * item1 = tableWidget_codecs->takeItem(currentRow, i);
		QTableWidgetItem * item2 = tableWidget_codecs->takeItem(currentRow - 1, i);
		tableWidget_codecs->setItem(currentRow - 1, i , item1);
		tableWidget_codecs->setItem(currentRow, i , item2);
	}
	tableWidget_codecs->setCurrentCell(currentRow - 1, currentCol);
}

void ConfigurationDialog::on_toolButton_codecDown_clicked()
{
	qDebug() << "on_toolButton_codecDown_clicked";
	int currentCol = tableWidget_codecs->currentColumn();
	int currentRow = tableWidget_codecs->currentRow();
	int nbCol = tableWidget_codecs->columnCount();
	for(int i = 0 ; i < nbCol ; i++)
	{
		QTableWidgetItem * item1 = tableWidget_codecs->takeItem(currentRow, i);
		QTableWidgetItem * item2 = tableWidget_codecs->takeItem(currentRow + 1, i);
		tableWidget_codecs->setItem(currentRow + 1, i , item1);
		tableWidget_codecs->setItem(currentRow, i , item2);
	}
	tableWidget_codecs->setCurrentCell(currentRow + 1, currentCol);
}

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
	qDebug() << "on_button_accountUp_clicked";
	int currentRow = listWidget_accountList->currentRow();
	QListWidgetItem * item = listWidget_accountList->takeItem(currentRow);
	listWidget_accountList->insertItem(currentRow - 1 , item);
	listWidget_accountList->setCurrentItem(item);
}

void ConfigurationDialog::on_button_accountDown_clicked()
{
	qDebug() << "on_button_accountDown_clicked";
	int currentRow = listWidget_accountList->currentRow();
	QListWidgetItem * item = listWidget_accountList->takeItem(currentRow);
	listWidget_accountList->insertItem(currentRow + 1 , item);
	listWidget_accountList->setCurrentItem(item);
}

void ConfigurationDialog::on_button_accountAdd_clicked()
{
	qDebug() << "on_button_accountAdd_clicked";
	QString itemName = QInputDialog::getText(this, "New account", "Enter new account's alias");
	itemName = itemName.simplified();
	if (!itemName.isEmpty()) {
		Account * account = accountList->addAccount(itemName);
		addAccountToAccountList(account);
		int r = listWidget_accountList->count() - 1;
		listWidget_accountList->setCurrentRow(r);
		frame2_editAccounts->setEnabled(true);
	}
}

void ConfigurationDialog::on_button_accountRemove_clicked()
{
	qDebug() << "on_button_accountRemove_clicked";
	int r = listWidget_accountList->currentRow();
	QListWidgetItem * item = listWidget_accountList->takeItem(r);
	accountList->removeAccount(item);
	listWidget_accountList->setCurrentRow( (r >= listWidget_accountList->count()) ? r-1 : r );
}


void ConfigurationDialog::on_buttonBoxDialog_clicked(QAbstractButton * button)
{
	qDebug() << "on_buttonBoxDialog_clicked";
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

void ConfigurationDialog::on1_accountsChanged()
{
	qDebug() << "on1_accountsChanged";
}

void ConfigurationDialog::on1_parametersChanged()
{
	qDebug() << "on1_parametersChanged";
}

void ConfigurationDialog::on1_errorAlert(int code)
{
	qDebug() << "on1_errorAlert code : " << code ;
}
