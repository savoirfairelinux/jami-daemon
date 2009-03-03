#include "AccountList.h"
#include "sflphone_const.h"


//Constructors
/*
AccountList::AccountList(VectorString & _accountIds)
{
	accounts = new QVector<Account *>(1);
	(*accounts) += new Account(*(new QListWidgetItem()), "alias");
	for (int i = 0; i < _accountIds.size(); ++i){
		(*accounts) += new Account(_accountIds[i]);
	}
}
*/
AccountList::AccountList(QStringList & _accountIds)
{
	accounts = new QVector<Account *>();
	for (int i = 0; i < _accountIds.size(); ++i){
		(*accounts) += Account::buildExistingAccountFromId(_accountIds[i]);
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

QVector<Account *> AccountList::getAccountByState(QString & state)
{
	QVector<Account *> v;
	for (int i = 0; i < accounts->size(); ++i){
		if ((*accounts)[i]->getAccountDetail(*(new QString(ACCOUNT_STATUS))) == state)
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
		if ( (*accounts)[i]->getItem() == item)
			return (*accounts)[i];
	}
	return NULL;
}

int AccountList::size()
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
QListWidgetItem * AccountList::addAccount(QString & alias)
{
	Account * a = Account::buildNewAccountFromAlias(alias);
	(*accounts) += a;
	return a->getItem();
}

void AccountList::removeAccount(QListWidgetItem * item)
{
	if(!item) {qDebug() << "Attempting to remove an account from a NULL item."; return; }

	Account * a = getAccountByItem(item);
	if(!a) {qDebug() << "Attempting to remove an unexisting account."; return; }

	accounts->remove(accounts->indexOf(a));
}

const Account & AccountList::operator[] (int i) const
{
	return *((*accounts)[i]);
}

Account & AccountList::operator[] (int i)
{
	return *((*accounts)[i]);
}
