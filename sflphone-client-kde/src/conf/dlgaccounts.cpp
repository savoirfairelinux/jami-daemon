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
#include "dlgaccounts.h"

#include <QtGui/QInputDialog>

#include "configurationmanager_interface_singleton.h"
#include "sflphone_kdeview.h"
#include "sflphone_const.h"
#include "conf/ConfigurationDialog.h"

DlgAccounts::DlgAccounts(KConfigDialog *parent)
 : QWidget(parent)
{
	setupUi(this);
	
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	button_accountUp->setIcon(KIcon("go-up"));
	button_accountDown->setIcon(KIcon("go-down"));
	button_accountAdd->setIcon(KIcon("list-add"));
	button_accountRemove->setIcon(KIcon("list-remove"));
	accountList = new AccountList(false);
	loadAccountList();
	accountListHasChanged = false;
	toolButton_accountsApply->setEnabled(false);
	
	connect(edit1_alias,           SIGNAL(textEdited(const QString &)),
	        this,                  SLOT(changedAccountList()));
	connect(edit2_protocol,        SIGNAL(currentIndexChanged(int)),
	        this,                  SLOT(changedAccountList()));
	connect(edit3_server,          SIGNAL(textEdited(const QString &)),
	        this,                  SLOT(changedAccountList()));
	connect(edit4_user,            SIGNAL(textEdited(const QString &)),
	        this,                  SLOT(changedAccountList()));
	connect(edit5_password,        SIGNAL(textEdited(const QString &)),
	        this,                  SLOT(changedAccountList()));
	connect(edit6_mailbox,         SIGNAL(textEdited(const QString &)),
	        this,                  SLOT(changedAccountList()));
	connect(button_accountUp,      SIGNAL(clicked()),
	        this,                  SLOT(changedAccountList()));
	connect(button_accountDown,    SIGNAL(clicked()),
	        this,                  SLOT(changedAccountList()));
	connect(button_accountAdd,     SIGNAL(clicked()),
	        this,                  SLOT(changedAccountList()));
	connect(button_accountRemove,  SIGNAL(clicked()),
	        this,                  SLOT(changedAccountList()));
	        
	connect(&configurationManager, SIGNAL(accountsChanged()),
	        this,                  SLOT(updateAccountStates()));
	        
	
	connect(this,     SIGNAL(updateButtons()), parent, SLOT(updateButtons()));
}


DlgAccounts::~DlgAccounts()
{
}

void DlgAccounts::saveAccountList()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	disconnectAccountsChangedSignal();
	//save the account being edited
	if(listWidget_accountList->currentItem())
		saveAccount(listWidget_accountList->currentItem());
	QStringList accountIds= QStringList(configurationManager.getAccountList().value());
	//create or update each account from accountList
	for (int i = 0; i < accountList->size(); i++){
		Account & current = (*accountList)[i];
		QString currentId;
		//if the account has no instanciated id, it has just been created in the client
		if(current.isNew())
		{
			MapStringString details = current.getAccountDetails();
			currentId = configurationManager.addAccount(details);
			current.setAccountId(currentId);
		}
		//if the account has an instanciated id but it's not in configurationManager
		else{
			if(! accountIds.contains(current.getAccountId()))
			{
				qDebug() << "The account with id " << current.getAccountId() << " doesn't exist. It might have been removed by another SFLphone client.";
				currentId = QString();
			}
			else
			{
				configurationManager.setAccountDetails(current.getAccountId(), current.getAccountDetails());
				currentId = QString(current.getAccountId());
			}
		}
		qDebug() << currentId << " : " << current.isChecked();
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
	configurationManager.setAccountsOrder(accountList->getOrderedList());
	connectAccountsChangedSignal();
}

void DlgAccounts::connectAccountsChangedSignal()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	connect(&configurationManager, SIGNAL(accountsChanged()),
	        this,                  SLOT(on1_accountsChanged()));
}

void DlgAccounts::disconnectAccountsChangedSignal()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	disconnect(&configurationManager, SIGNAL(accountsChanged()),
	        this,                  SLOT(on1_accountsChanged()));
}


void DlgAccounts::saveAccount(QListWidgetItem * item)
{
	if(! item)  { qDebug() << "Attempting to save details of an account from a NULL item"; return; }
	
	Account * account = accountList->getAccountByItem(item);
	if(! account)  {  qDebug() << "Attempting to save details of an unexisting account : " << item->text();  return;  }

	account->setAccountDetail(ACCOUNT_ALIAS, edit1_alias->text());
	QString protocolsTab[] = ACCOUNT_TYPES_TAB;
	account->setAccountDetail(ACCOUNT_TYPE, protocolsTab[edit2_protocol->currentIndex()]);
	account->setAccountDetail(ACCOUNT_HOSTNAME, edit3_server->text());
	account->setAccountDetail(ACCOUNT_USERNAME, edit4_user->text());
	account->setAccountDetail(ACCOUNT_PASSWORD, edit5_password->text());
	account->setAccountDetail(ACCOUNT_MAILBOX, edit6_mailbox->text());
	account->setAccountDetail(ACCOUNT_ENABLED, account->isChecked() ? ACCOUNT_ENABLED_TRUE : ACCOUNT_ENABLED_FALSE);
}

void DlgAccounts::loadAccount(QListWidgetItem * item)
{
	if(! item )  {  qDebug() << "Attempting to load details of an account from a NULL item";  return;  }

	Account * account = accountList->getAccountByItem(item);
	if(! account )  {  qDebug() << "Attempting to load details of an unexisting account";  return;  }

	edit1_alias->setText( account->getAccountDetail(ACCOUNT_ALIAS));
	
	QString protocolsTab[] = ACCOUNT_TYPES_TAB;
	QList<QString> * protocolsList = new QList<QString>();
	for(int i = 0 ; i < (int) (sizeof(protocolsTab) / sizeof(QString)) ; i++)
	{ 
		protocolsList->append(protocolsTab[i]);
	}
	QString accountName = account->getAccountDetail(ACCOUNT_TYPE);
	int protocolIndex = protocolsList->indexOf(accountName);
	delete protocolsList;
	
	edit2_protocol->setCurrentIndex( (protocolIndex < 0) ? 0 : protocolIndex );
	edit3_server->setText( account->getAccountDetail(ACCOUNT_HOSTNAME));
	edit4_user->setText( account->getAccountDetail(ACCOUNT_USERNAME));
	edit5_password->setText( account->getAccountDetail(ACCOUNT_PASSWORD));
	edit6_mailbox->setText( account->getAccountDetail(ACCOUNT_MAILBOX));
	QString status = account->getAccountDetail(ACCOUNT_STATUS);
	edit7_state->setText( "<FONT COLOR=\"" + account->getStateColorName() + "\">" + status + "</FONT>" );
	frame2_editAccounts->setEnabled(true);
}

void DlgAccounts::loadAccountList()
{
	qDebug() << "loadAccountList";
	accountList->updateAccounts();
	//initialize the QListWidget object with the AccountList
	listWidget_accountList->clear();
	for (int i = 0; i < accountList->size(); ++i){
		addAccountToAccountList(&(*accountList)[i]);
	}
	if (listWidget_accountList->count() > 0 && listWidget_accountList->currentItem() == NULL) 
		listWidget_accountList->setCurrentRow(0);
	else 
		frame2_editAccounts->setEnabled(false);
}

void DlgAccounts::addAccountToAccountList(Account * account)
{
	qDebug() << "addAccountToAccountList";
	QListWidgetItem * item = account->getItem();
	QWidget * widget = account->getItemWidget();
	connect(widget, SIGNAL(checkStateChanged(bool)),
	        this,   SLOT(changedAccountList()));
	qDebug() << "item->isHidden()" << item->isHidden();
	listWidget_accountList->addItem(item);
	qDebug() << "addAccountToAccountList2";
	listWidget_accountList->setItemWidget(item, widget);
}

void DlgAccounts::changedAccountList()
{
	qDebug() << "changedAccountList";
	accountListHasChanged = true;
	emit updateButtons();
	toolButton_accountsApply->setEnabled(hasChanged());
}



void DlgAccounts::on_listWidget_accountList_currentItemChanged ( QListWidgetItem * current, QListWidgetItem * previous )
{
	qDebug() << "on_listWidget_accountList_currentItemChanged";
	saveAccount(previous);
	loadAccount(current);
	updateAccountListCommands();
}

void DlgAccounts::on_button_accountUp_clicked()
{
	qDebug() << "on_button_accountUp_clicked";
	int currentRow = listWidget_accountList->currentRow();
	QListWidgetItem * prevItem = listWidget_accountList->takeItem(currentRow);
	Account * account = accountList->getAccountByItem(prevItem);
	//we need to build a new item to set the itemWidget back
	account->initItem();
	QListWidgetItem * item = account->getItem();
	AccountItemWidget * widget = account->getItemWidget();
	accountList->upAccount(currentRow);
	listWidget_accountList->insertItem(currentRow - 1 , item);
	listWidget_accountList->setItemWidget(item, widget);
	listWidget_accountList->setCurrentItem(item);
// 	changedAccountList();
}

void DlgAccounts::on_button_accountDown_clicked()
{
	qDebug() << "on_button_accountDown_clicked";
	int currentRow = listWidget_accountList->currentRow();
	QListWidgetItem * prevItem = listWidget_accountList->takeItem(currentRow);
	Account * account = accountList->getAccountByItem(prevItem);
	//we need to build a new item to set the itemWidget back
	account->initItem();
	QListWidgetItem * item = account->getItem();
	AccountItemWidget * widget = account->getItemWidget();
	accountList->downAccount(currentRow);
	listWidget_accountList->insertItem(currentRow + 1 , item);
	listWidget_accountList->setItemWidget(item, widget);
	listWidget_accountList->setCurrentItem(item);
// 	changedAccountList();
}

void DlgAccounts::on_button_accountAdd_clicked()
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
// 	changedAccountList();
}

void DlgAccounts::on_button_accountRemove_clicked()
{
	qDebug() << "on_button_accountRemove_clicked";
	int r = listWidget_accountList->currentRow();
	QListWidgetItem * item = listWidget_accountList->takeItem(r);
	accountList->removeAccount(item);
	listWidget_accountList->setCurrentRow( (r >= listWidget_accountList->count()) ? r-1 : r );
// 	changedAccountList();
}

void DlgAccounts::on_toolButton_accountsApply_clicked()
{
	qDebug() << "on_toolButton_accountsApply_clicked";
	applyCustomSettings();
}

void DlgAccounts::applyCustomSettings()
{
	qDebug() << "DlgAccounts::applyCustomSettings";
	if(hasChanged())
	{
		toolButton_accountsApply->setEnabled(false);
		saveAccountList();
		loadAccountList();
		accountListHasChanged = false;
	}
}

void DlgAccounts::on_edit1_alias_textChanged(const QString & text)
{
	qDebug() << "on_edit1_alias_textChanged";
	AccountItemWidget * widget = (AccountItemWidget *) listWidget_accountList->itemWidget(listWidget_accountList->currentItem());
	widget->setAccountText(text);
}

void DlgAccounts::updateAccountListCommands()
{
	qDebug() << "updateAccountListCommands";
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
	if(listWidget_accountList->currentRow() == listWidget_accountList->count() - 1)
	{
		buttonsEnabled[1] = false;
	}
	button_accountUp->setEnabled(buttonsEnabled[0]);
	button_accountDown->setEnabled(buttonsEnabled[1]);
	button_accountAdd->setEnabled(buttonsEnabled[2]);
	button_accountRemove->setEnabled(buttonsEnabled[3]);
}

void DlgAccounts::updateAccountStates()
{
	qDebug() << "updateAccountStates";
	qDebug() << accountList->size();
	for (int i = 0; i < accountList->size(); i++)
	{
		Account & current = accountList->getAccount(i);
		current.updateState();
	}
	qDebug() << accountList->size();
}


bool DlgAccounts::hasChanged()
{
// 	qDebug() << "DlgAudio::hasChanged";
	return accountListHasChanged;
}


void DlgAccounts::updateSettings()
{

}
void DlgAccounts::updateWidgets()
{
	loadAccountList();
	accountListHasChanged = false;
}

