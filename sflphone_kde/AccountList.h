#ifndef ACCOUNT_LIST_H
#define ACCOUNT_LIST_H

#include <QtGui>
#include "Account.h"

class AccountList{
	
private:

	QVector<Account *> * accounts;

public:

	//Constructors
	AccountList(QStringList & _accountIds);
	~AccountList();
	
	//Getters
	QVector<Account *> & getAccounts();
	Account * getAccountById(QString & id);
	QVector<Account *>  getAccountByState(QString & state);
	Account * getAccountByItem(QListWidgetItem * item);
	int size();
	
	//Setters
	Account * addAccount(QString & alias);
	void removeAccount(QListWidgetItem * item);

	//Operators
	Account & operator[] (int i);
	const Account & operator[] (int i) const;
};




#endif