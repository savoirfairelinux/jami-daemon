#ifndef ACCOUNT_LIST_H
#define ACCOUNT_LIST_H


#include <QtCore/QVector>

#include "Account.h"

class AccountList{
	
private:

	QVector<Account *> * accounts;
	Account * firstAccount;

public:

	//Constructors
	AccountList(QStringList & _accountIds);
	AccountList();
	~AccountList();
	
	//Getters
	QVector<Account *> & getAccounts();
	Account * getAccountById(QString & id);
	QVector<Account *>  getAccountByState(QString & state);
	Account * getAccountByItem(QListWidgetItem * item);
	int size();
	Account * firstRegisteredAccount() const;
	
	//Setters
	Account * addAccount(QString & alias);
	void removeAccount(Account * account);
	void removeAccount(QListWidgetItem * item);
	void setAccountFirst(Account * account);

	//Operators
	Account & operator[] (int i);
	const Account & operator[] (int i) const;
	QVector<Account *> registeredAccounts() const;
	void update();
};




#endif