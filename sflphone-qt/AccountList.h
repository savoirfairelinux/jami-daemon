#ifndef HEADER_ACCOUNTLIST
#define HEADER_ACCOUNTLIST

#include <QtGui>
#include "Account.h"

class AccountList{
	
private:

	QVector<Account *> * accounts;

public:

	//Constructors
	AccountList(VectorString & _accountIds);
	AccountList(QStringList & _accountIds);
	~AccountList();
	
	//Getters
	QVector<Account *> & getAccounts();
	Account * getAccountById(QString & id);
	QVector<Account *>  getAccountByState(account_state_t & id);
	//Account * getAccountByRow(int row);
	Account * getAccountByItem(QListWidgetItem * item);
	int getSize();
	
	//Setters
	//void addAccount(Account & account);
	void addAccount(QListWidgetItem & _item, QString & alias);
};




#endif