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

#ifndef ACCOUNT_LIST_H
#define ACCOUNT_LIST_H


#include <QtCore/QVector>

#include "Account.h"

class AccountList{
	
private:

	QVector<Account *> * accounts;
	static QString firstAccount;

public:

	//Constructors & Destructors
	AccountList(QStringList & _accountIds);
	AccountList();
	~AccountList();
	
	//Getters
	QVector<Account *> & getAccounts();
	Account & getAccount (int i);
	const Account & getAccount (int i) const;
	Account * getAccountById(const QString & id) const;
	QVector<Account *>  getAccountByState(QString & state);
	Account * getAccountByItem(QListWidgetItem * item);
	int size();
	Account * firstRegisteredAccount() const;
	QString getOrderedList();
	
	//Setters
	Account * addAccount(QString & alias);
	void removeAccount(Account * account);
	void removeAccount(QListWidgetItem * item);
	void setAccountFirst(Account * account);
	void upAccount(int index);
	void downAccount(int index);

	//Operators
	Account & operator[] (int i);
	const Account & operator[] (int i) const;
	QVector<Account *> registeredAccounts() const;
	void update();
};


#endif