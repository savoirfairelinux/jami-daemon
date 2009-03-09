#include "SFLPhone.h"
#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"
#include "callmanager_interface_singleton.h"
#include <stdlib.h>

SFLPhone::SFLPhone(QMainWindow *parent) : QMainWindow(parent),callIdCpt(0)
{
    setupUi(this);
    
    configDialog = new ConfigurationDialog(this);
    configDialog->setModal(true);
    
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
}

void SFLPhone::on_actionAfficher_les_barres_de_volume_toggled()
{
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	daemon.setVolumeControls();
}

void SFLPhone::on_actionAfficher_le_clavier_toggled()
{
	ConfigurationManagerInterface & daemon = ConfigurationManagerInterfaceSingleton::getInstance();
	daemon.setDialpad();
}


void SFLPhone::typeChar(QChar c)
{
	QListWidgetItem * item = listWidget_callList->currentItem();
	if(!item)
	{
		listWidget_callList->addItem(QString(c));
		listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
	}
	else
	{
		listWidget_callList->currentItem()->setText(listWidget_callList->currentItem()->text() + c);
	}
}



void SFLPhone::on_pushButton_1_clicked(){ typeChar('1'); }
void SFLPhone::on_pushButton_2_clicked(){ typeChar('2'); }
void SFLPhone::on_pushButton_3_clicked(){ typeChar('3'); }
void SFLPhone::on_pushButton_4_clicked(){ typeChar('4'); }
void SFLPhone::on_pushButton_5_clicked(){ typeChar('5'); }
void SFLPhone::on_pushButton_6_clicked(){ typeChar('6'); }
void SFLPhone::on_pushButton_7_clicked(){ typeChar('7'); }
void SFLPhone::on_pushButton_8_clicked(){ typeChar('8'); }
void SFLPhone::on_pushButton_9_clicked(){ typeChar('9'); }
void SFLPhone::on_pushButton_0_clicked(){ typeChar('0'); }
void SFLPhone::on_pushButton_diese_clicked(){ typeChar('#'); }
void SFLPhone::on_pushButton_etoile_clicked(){ typeChar('*'); }


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
	CallManagerInterface & daemon = CallManagerInterfaceSingleton::getInstance();
	QListWidgetItem * item = listWidget_callList->currentItem();
	if(!item)
	{
		qDebug() << "Calling when no item is selected. Opening an item.";
		listWidget_callList->addItem(QString(""));
		listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
	}
	else
	{
		qDebug() << "Calling " << item->text() << " with account " << firstAccount() << ". callId : " << QString::number(callIdCpt);
		daemon.placeCall(firstAccount(), getCallId(), item->text());
	}
}

void SFLPhone::on_actionRaccrocher_triggered()
{
	CallManagerInterface & daemon = CallManagerInterfaceSingleton::getInstance();
	QListWidgetItem * item = listWidget_callList->currentItem();
	if(!item)
	{
		qDebug() << "Hanging up when no item selected. Should not happen.";
	}
	else
	{
		Call * call = callList[item];
		if(!call) return;
		if(call->getState() == INCOMING)
		{
			qDebug() << "Refusing call from " << item->text() << " with account " << firstAccount() << ". callId : " << QString::number(callIdCpt);
			daemon.refuse(getCallId());
		}
		else
		{
			qDebug() << "Hanging up with " << item->text() << " with account " << firstAccount() << ". callId : " << QString::number(callIdCpt);
			daemon.hangUp(getCallId());
		}
	}
}

void SFLPhone::on_actionMettre_en_attente_triggered()
{

}

void SFLPhone::on_actionTransferer_triggered()
{

}

void SFLPhone::on_actionHistorique_triggered()
{

}

void SFLPhone::on_actionBoite_vocale_triggered()
{

}

void SFLPhone::on_actionAbout()
{

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

QString getCallId()
{
	return QString::number(callIdCpt++);
}
