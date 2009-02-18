#include "AccountList.h"


//Constructors

AccountList::AccountList(VectorString & _accountIds)
{
	accounts = new QVector<Account *>();
	for (int i = 0; i < _accountIds.size(); ++i){
		(*accounts) += new Account(_accountIds[i]);
	}
}

AccountList::AccountList(QStringList & _accountIds)
{
	accounts = new QVector<Account *>();
	for (int i = 0; i < _accountIds.size(); ++i){
		(*accounts) += new Account(_accountIds[i]);
	}
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

Account * AccountList::getAccountById(QString & id)
{
	for (int i = 0; i < accounts->size(); ++i){
		if ((*accounts)[i]->getAccountId() == id)
			return (*accounts)[i];
	}
	return NULL;
}

QVector<Account *> AccountList::getAccountByState(account_state_t & state)
{
	QVector<Account *> v;
	for (int i = 0; i < accounts->size(); ++i){
		if ((*accounts)[i]->getState() == state)
			v += (*accounts)[i];
	}
	return v;

}
/*
Account AccountList::getAccountByRow(int row)
{
	
}
*/
Account * AccountList::getAccountByItem(QListWidgetItem * item)
{
	for (int i = 0; i < accounts->size(); ++i){
		if ( &(*accounts)[i]->getItem() == item)
			return (*accounts)[i];
	}
	return NULL;
}

int AccountList::getSize()
{
	return accounts->size();
}

//Setters
/*
void AccountList::addAccount(Account & account)
{
	accounts->add(account);
}
*/
void AccountList::addAccount(QListWidgetItem & _item, QString & alias)
{
	(*accounts) += new Account(_item, alias);
}
