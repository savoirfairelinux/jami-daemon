#ifndef HEADER_ACCOUNTLIST
#define HEADER_ACCOUNTLIST

#include <QtGui>
#include "Account.h"

class AccountList{
	
private:

	QVector<Account *> * accounts;

public:

	//Constructors
	//AccountList(VectorString & _accountIds);
	AccountList(QStringList & _accountIds);
	~AccountList();
	
	//Getters
	QVector<Account *> & getAccounts();
	Account * getAccountById(QString & id);
	QVector<Account *>  getAccountByState(QString & state);
	//Account * getAccountByRow(int row);
	Account * getAccountByItem(QListWidgetItem * item);
	int size();
	
	//Setters
	//void addAccount(Account & account);
	QListWidgetItem * addAccount(QString & alias);
	void removeAccount(QListWidgetItem * item);

	//Operators
	Account & operator[] (int i);
	const Account & operator[] (int i) const;
};




#endif