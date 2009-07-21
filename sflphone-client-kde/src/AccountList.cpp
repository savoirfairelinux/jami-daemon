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

#include "AccountList.h"
#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"



//Constructors

AccountList::AccountList(QStringList & _accountIds)
{
// 	firstAccount = QString();
	accounts = new QVector<Account *>();
	for (int i = 0; i < _accountIds.size(); ++i){
		(*accounts) += Account::buildExistingAccountFromId(_accountIds[i]);
	}
}

AccountList::AccountList(bool fill)
{
	accounts = new QVector<Account *>();
	if(fill)
	{
		updateAccounts();
	}
}

void AccountList::update()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	Account * current;
	for (int i = 0; i < accounts->size(); i++){
		current = (*accounts)[i];
		if (!(*accounts)[i]->isNew())
			removeAccount(current);
	}
	//ask for the list of accounts ids to the configurationManager
	QStringList accountIds = configurationManager.getAccountList().value();
	for (int i = 0; i < accountIds.size(); ++i){
		accounts->insert(i, Account::buildExistingAccountFromId(accountIds[i]));
	}
}

void AccountList::updateAccounts()
{
	qDebug() << "updateAccounts";
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	QStringList accountIds = configurationManager.getAccountList().value();
	accounts->clear();
	for (int i = 0; i < accountIds.size(); ++i){
		(*accounts) += Account::buildExistingAccountFromId(accountIds[i]);
	}
	emit accountListUpdated();
}

void AccountList::upAccount(int index)
{
	if(index <= 0 || index >= size())
	{
		qDebug() << "Error : index or future index out of range in upAccount.";
		return;
	}
	Account * account = getAccountAt(index);
	accounts->remove(index);
	accounts->insert(index - 1, account);
}

void AccountList::downAccount(int index)
{
	if(index < 0 || index >= size() - 1)
	{
		qDebug() << "Error : index or future index out of range in upAccount.";
		return;
	}
	Account * account = getAccountAt(index);
	accounts->remove(index);
	accounts->insert(index + 1, account);
}


QString AccountList::getOrderedList() const
{
	QString order;
	for( int i = 0 ; i < size() ; i++)
	{
		order += getAccountAt(i)->getAccountId() + "/";
	}
	return order;
}

QVector<Account *> AccountList::registeredAccounts() const
{
	qDebug() << "registeredAccounts";
	QVector<Account *> registeredAccounts;
	Account * current;
	for (int i = 0; i < accounts->count(); ++i){
		current = (*accounts)[i];
		if(current->getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED)
		{
			qDebug() << current->getAlias() << " : " << current;
			registeredAccounts.append(current);
		}
	}
	return registeredAccounts;
}

Account * AccountList::firstRegisteredAccount() const
{
	Account * current;
	for (int i = 0; i < accounts->count(); ++i){
		current = (*accounts)[i];
		if(current->getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED)
		{
			return current;
		}
	}
	return NULL;
}


AccountList::~AccountList()
{
	delete accounts;
}

//Getters
QVector<Account *> & AccountList::getAccounts()
{
	return *accounts;
}

const Account * AccountList::getAccountAt (int i) const
{
	return (*accounts)[i];
}

Account * AccountList::getAccountAt (int i)
{
	return (*accounts)[i];
}

Account * AccountList::getAccountById(const QString & id) const
{
	if(id.isEmpty())
	{	return NULL;	}
	for (int i = 0; i < accounts->size(); ++i)
	{
		if (!(*accounts)[i]->isNew() && (*accounts)[i]->getAccountId() == id)
		{
			return (*accounts)[i];
		}
	}
	return NULL;
}

QVector<Account *> AccountList::getAccountByState(QString & state)
{
	QVector<Account *> v;
	for (int i = 0; i < accounts->size(); ++i){
		if ((*accounts)[i]->getAccountDetail(ACCOUNT_STATUS) == state)
			v += (*accounts)[i];
	}
	return v;
}


Account * AccountList::getAccountByItem(QListWidgetItem * item)
{
	for (int i = 0; i < accounts->size(); ++i){
		if ( (*accounts)[i]->getItem() == item)
			return (*accounts)[i];
	}
	return NULL;
}

int AccountList::size() const
{
	return accounts->size();
}

//Setters
Account * AccountList::addAccount(QString & alias)
{
	Account * a = Account::buildNewAccountFromAlias(alias);
	(*accounts) += a;
	return a;
}

void AccountList::removeAccount(QListWidgetItem * item)
{
	if(!item) {qDebug() << "Attempting to remove an account from a NULL item."; return; }

	Account * a = getAccountByItem(item);
	if(!a) {qDebug() << "Attempting to remove an unexisting account."; return; }

	accounts->remove(accounts->indexOf(a));
}

void AccountList::removeAccount(Account * account)
{
	accounts->remove(accounts->indexOf(account));
}

const Account * AccountList::operator[] (int i) const
{
	return (*accounts)[i];
}

Account * AccountList::operator[] (int i)
{
	return (*accounts)[i];
}
