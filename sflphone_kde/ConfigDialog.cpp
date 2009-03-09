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
	errorWindow = new QErrorMessage(this);
	codecPayloads = new MapStringString();
	horizontalSlider_Capacity->setMaximum(MAX_HISTORY_CAPACITY);
	label_WarningSIP->setVisible(false);

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
	spinBox_CapaciteHist->setValue(daemon.getMaxCalls());
	
	//SIP port settings
	int sipPort = daemon.getSipPort();
	if(sipPort<1025){
		spinBox_PortSIP->setMinimum(sipPort);
		label_WarningSIP->setText("Attention : le port SIP doit être supérieur à 1024 !");
		label_WarningSIP->setVisible(true);
	}
	if(sipPort>65535){
		spinBox_PortSIP->setMaximum(sipPort);
		label_WarningSIP->setText("Attention : le port SIP doit être inférieur à 65536 !");
		label_WarningSIP->setVisible(true);
	}
	spinBox_PortSIP->setValue(daemon.getSipPort());
	
	////////////////////////
	////Display settings////
	////////////////////////

	//Notification settings
	checkBox1_NotifAppels->setCheckState(daemon.getNotify() ? Qt::Checked : Qt::Unchecked);
	checkBox2_NotifMessages->setCheckState(daemon.getMailNotify() ? Qt::Checked : Qt::Unchecked);
	
	//Window display settings
	checkBox1_FenDemarrage->setCheckState(daemon.isStartHidden() ? Qt::Unchecked : Qt::Checked);
	checkBox2_FenAppel->setCheckState(daemon.popupMode() ? Qt::Checked : Qt::Unchecked);
	
	/////////////////////////
	////Accounts settings////
	/////////////////////////
	
	loadAccountList();

	//Stun settings
	checkBoxStun->setCheckState(daemon.isStunEnabled() ? Qt::Checked : Qt::Unchecked);
	lineEdit_Stun->setText(QString(daemon.getStunServer()));
	
	//////////////////////
	////Audio settings////
	//////////////////////
	
	//Audio Interface settings
	comboBox_Interface->setCurrentIndex(daemon.getAudioManager());
	stackedWidget_ParametresSpecif->setCurrentIndex(daemon.getAudioManager());
	
	//ringtones settings
	checkBox_Sonneries->setCheckState(daemon.isRingtoneEnabled() ? Qt::Checked : Qt::Unchecked);
	//TODO widget choix de sonnerie
	//widget_nomSonnerie->setText(daemon.getRingtoneChoice());
	
	//codecs settings
	loadCodecs();

	//
	//alsa settings
	comboBox1_GreffonAlsa->clear();
	QStringList pluginList = daemon.getOutputAudioPluginList();
	comboBox1_GreffonAlsa->addItems(pluginList);
	comboBox1_GreffonAlsa->setCurrentIndex(comboBox1_GreffonAlsa->findText(daemon.getCurrentAudioOutputPlugin()));
	
	qDebug() << "avant daemon.getCurrentAudioDevicesIndex();";
	QStringList devices = daemon.getCurrentAudioDevicesIndex();
	qDebug() << "apres daemon.getCurrentAudioDevicesIndex();";

	int inputDevice = devices[1].toInt();
	comboBox2_Entree->clear();
	QStringList inputDeviceList = daemon.getAudioInputDeviceList();
	comboBox2_Entree->addItems(inputDeviceList);
	comboBox2_Entree->setCurrentIndex(inputDevice);
	
	int outputDevice = devices[0].toInt();
	comboBox3_Sortie->clear();
	QStringList outputDeviceList = daemon.getAudioOutputDeviceList();
	comboBox3_Sortie->addItems(inputDeviceList);
	comboBox3_Sortie->setCurrentIndex(outputDevice);
	
	//pulseaudio settings
	checkBox_ModifVolumeApps->setCheckState(daemon.getPulseAppVolumeControl() ? Qt::Checked : Qt::Unchecked);
	
	
}


void ConfigurationDialog::saveOptions()
{
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	
	////////////////////////
	////General settings////
	////////////////////////
	
	//Call history settings
	daemon.setMaxCalls(spinBox_CapaciteHist->value());
	
	//SIP port settings
	int sipPort = spinBox_PortSIP->value();
	
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
	if(checkBox1_NotifAppels->checkState() != (daemon.getNotify() ? Qt::Checked : Qt::Unchecked)) daemon.setNotify();
	if(checkBox2_NotifMessages->checkState() != (daemon.getMailNotify() ? Qt::Checked : Qt::Unchecked)) daemon.setMailNotify();
	
	//Window display settings
	//WARNING états inversés
	if(checkBox1_FenDemarrage->checkState() != (daemon.isStartHidden() ? Qt::Unchecked : Qt::Checked)) daemon.startHidden();
	if(checkBox2_FenAppel->checkState() != (daemon.popupMode() ? Qt::Checked : Qt::Unchecked)) daemon.switchPopupMode();
	
	/////////////////////////
	////Accounts settings////
	/////////////////////////
	
	saveAccountList();

	//Stun settings
	if(checkBoxStun->checkState() != (daemon.isStunEnabled() ? Qt::Checked : Qt::Unchecked)) daemon.enableStun();
	daemon.setStunServer(lineEdit_Stun->text());

	//////////////////////
	////Audio settings////
	//////////////////////
	
	//Audio Interface settings
	qDebug() << "setting audio manager";
	int manager = comboBox_Interface->currentIndex();
	daemon.setAudioManager(manager);
	
	//ringtones settings
	qDebug() << "setting ringtone options";
	if(checkBox_Sonneries->checkState() != (daemon.isRingtoneEnabled() ? Qt::Checked : Qt::Unchecked)) daemon.ringtoneEnabled();
	//TODO widget choix de sonnerie
	//daemon.getRingtoneChoice(widget_nomSonnerie->text());
	
	//codecs settings
	qDebug() << "saving codecs";
	saveCodecs();

	//alsa settings
	if(manager == ALSA)
	{
		qDebug() << "setting alsa settings";
		daemon.setOutputAudioPlugin(comboBox1_GreffonAlsa->currentText());
		daemon.setAudioInputDevice(comboBox2_Entree->currentIndex());
		daemon.setAudioOutputDevice(comboBox3_Sortie->currentIndex());
	}
	//pulseaudio settings
	if(manager == PULSEAUDIO)
	{
		qDebug() << "setting pulseaudio settings";
		if(checkBox_ModifVolumeApps->checkState() != (daemon.getPulseAppVolumeControl() ? Qt::Checked : Qt::Unchecked)) daemon.setPulseAppVolumeControl();
	}
}


void ConfigurationDialog::loadAccountList()
{
	//ask for the list of accounts ids to the daemon
	QStringList accountIds = ConfigurationManagerInterfaceSingleton::getInstance().getAccountList().value();
	//create the AccountList object with the ids
	accountList = new AccountList(accountIds);
	//initialize the QListWidget object with the AccountList
	listWidgetComptes->clear();
	for (int i = 0; i < accountList->size(); ++i){
		listWidgetComptes->addItem((*accountList)[i].getItem());
	}
	if (listWidgetComptes->count() > 0) 
		listWidgetComptes->setCurrentRow(0);
	else 
		frame2_EditComptes->setEnabled(false);
}

void ConfigurationDialog::saveAccountList()
{
	//save the account being edited
	if(listWidgetComptes->currentItem())
		saveAccount(listWidgetComptes->currentItem());
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

	edit1_Alias->setText( account->getAccountDetail(*(new QString(ACCOUNT_ALIAS))));
	int protocoleIndex = getProtocoleIndex(account->getAccountDetail(*(new QString(ACCOUNT_TYPE))));
	edit2_Protocole->setCurrentIndex( (protocoleIndex < 0) ? 0 : protocoleIndex );
	edit3_Serveur->setText( account->getAccountDetail(*(new QString(ACCOUNT_HOSTNAME))));
	edit4_Usager->setText( account->getAccountDetail(*(new QString(ACCOUNT_USERNAME))));
	edit5_Mdp->setText( account->getAccountDetail(*(new QString(ACCOUNT_PASSWORD))));
	edit6_BoiteVocale->setText( account->getAccountDetail(*(new QString(ACCOUNT_MAILBOX))));
	QString status = account->getAccountDetail(*(new QString(ACCOUNT_STATUS)));
	edit7_Etat->setText( "<FONT COLOR=\"" + account->getStateColorName() + "\">" + status + "</FONT>" );
	//edit7_Etat->setTextColor( account->getStateColor );
}


void ConfigurationDialog::saveAccount(QListWidgetItem * item)
{
	if(! item)  { qDebug() << "Attempting to save details of an account from a NULL item"; return; }
	
	Account * account = accountList->getAccountByItem(item);
	if(! account)  {  qDebug() << "Attempting to save details of an unexisting account : " << item->text(); return;  }

	account->setAccountDetail(ACCOUNT_ALIAS, edit1_Alias->text());
	account->setAccountDetail(ACCOUNT_TYPE, getIndexProtocole(edit2_Protocole->currentIndex()));
	account->setAccountDetail(ACCOUNT_HOSTNAME, edit3_Serveur->text());
	account->setAccountDetail(ACCOUNT_USERNAME, edit4_Usager->text());
	account->setAccountDetail(ACCOUNT_PASSWORD, edit5_Mdp->text());
	account->setAccountDetail(ACCOUNT_MAILBOX, edit6_BoiteVocale->text());
	
}


void ConfigurationDialog::loadCodecs()
{
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	QStringList codecList = daemon.getCodecList();
	QStringList activeCodecList = daemon.getActiveCodecList();
	qDebug() << codecList;
	qDebug() << activeCodecList;
	tableWidget_Codecs->setRowCount(0);
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
			tableWidget_Codecs->insertRow(i);
			QTableWidgetItem * headerItem = new QTableWidgetItem("");
			tableWidget_Codecs->setVerticalHeaderItem (i, headerItem);
			//headerItem->setVisible(false);
			tableWidget_Codecs->setItem(i,0,new QTableWidgetItem(""));
			tableWidget_Codecs->setItem(i,1,new QTableWidgetItem(details[CODEC_NAME]));
			//qDebug() << "tableWidget_Codecs->itemAt(" << i << "," << 2 << ")->setText(" << details[CODEC_NAME] << ");";
			//tableWidget_Codecs->item(i,2)->setText(details[CODEC_NAME]);
			tableWidget_Codecs->setItem(i,2,new QTableWidgetItem(details[CODEC_SAMPLE_RATE]));
			tableWidget_Codecs->setItem(i,3,new QTableWidgetItem(details[CODEC_BIT_RATE]));
			tableWidget_Codecs->setItem(i,4,new QTableWidgetItem(details[CODEC_BANDWIDTH]));
			tableWidget_Codecs->item(i,0)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
			tableWidget_Codecs->item(i,0)->setCheckState(activeCodecList.contains(codecList[i]) ? Qt::Checked : Qt::Unchecked);
			tableWidget_Codecs->item(i,1)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
			tableWidget_Codecs->item(i,2)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
			tableWidget_Codecs->item(i,3)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
			tableWidget_Codecs->item(i,4)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
			(*codecPayloads)[details[CODEC_NAME]] = payloadStr;
			qDebug() << "Added to codecs : " << payloadStr << " , " << details[CODEC_NAME];
		}
	}
	tableWidget_Codecs->resizeColumnsToContents();
	tableWidget_Codecs->resizeRowsToContents();
}


void ConfigurationDialog::saveCodecs()
{
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	QStringList activeCodecs;
	for(int i = 0 ; i < tableWidget_Codecs->rowCount() ; i++)
	{
		//qDebug() << "Checked if active : " << tableWidget_Codecs->item(i,1)->text();
		if(tableWidget_Codecs->item(i,0)->checkState() == Qt::Checked)
		{
			//qDebug() << "Added to activeCodecs : " << tableWidget_Codecs->item(i,1)->text();
			activeCodecs << (*codecPayloads)[tableWidget_Codecs->item(i,1)->text()];
		}
	}
	qDebug() << "Calling setActiveCodecList with list : " << activeCodecs ;
	daemon.setActiveCodecList(activeCodecs);
}

void ConfigurationDialog::setPage(int page)
{
	stackedWidgetOptions->setCurrentIndex(page);
	listOptions->setCurrentRow(page);
}

void ConfigurationDialog::on_edit1_Alias_textChanged(const QString & text)
{
	listWidgetComptes->currentItem()->setText(text);
}


void ConfigurationDialog::on_listWidgetComptes_currentItemChanged ( QListWidgetItem * current, QListWidgetItem * previous )
{
	if(previous)
		saveAccount(previous);
	if(current)
		loadAccount(current);
}

void ConfigurationDialog::on_spinBox_PortSIP_valueChanged ( int value )
{
	if(value>1024 && value<65536)
		label_WarningSIP->setVisible(false);
	else
		label_WarningSIP->setVisible(true);
}



void ConfigurationDialog::on_buttonNouveauCompte_clicked()
{
	QString itemName = QInputDialog::getText(this, "Item", "Enter new item");
	itemName = itemName.simplified();
	if (!itemName.isEmpty()) {
		QListWidgetItem * item = accountList->addAccount(itemName);
		//TODO verifier que addItem set bien le parent
		listWidgetComptes->addItem(item);
		int r = listWidgetComptes->count() - 1;
		listWidgetComptes->setCurrentRow(r);
		frame2_EditComptes->setEnabled(true);
	}
}

void ConfigurationDialog::on_buttonSupprimerCompte_clicked()
{
	int r = listWidgetComptes->currentRow();
	QListWidgetItem * item = listWidgetComptes->takeItem(r);
	accountList->removeAccount(item);
	listWidgetComptes->setCurrentRow( (r >= listWidgetComptes->count()) ? r-1 : r );
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