#include "AccountList.h"
#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"

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
	firstAccount = NULL;
	accounts = new QVector<Account *>();
	for (int i = 0; i < _accountIds.size(); ++i){
		(*accounts) += Account::buildExistingAccountFromId(_accountIds[i]);
	}
}

AccountList::AccountList()
{
	firstAccount = NULL;
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	//ask for the list of accounts ids to the configurationManager
	QStringList accountIds = configurationManager.getAccountList().value();
	accounts = new QVector<Account *>();
	for (int i = 0; i < accountIds.size(); ++i){
		(*accounts) += Account::buildExistingAccountFromId(accountIds[i]);
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

QVector<Account *> AccountList::registeredAccounts() const
{
	QVector<Account *> registeredAccounts;
	Account * current;
	for (int i = 0; i < accounts->count(); ++i){
		current = (*accounts)[i];
		if(current->getAccountDetail(ACCOUNT_STATUS) == QString(ACCOUNT_STATE_REGISTERED))
		{
			registeredAccounts.append(current);
		}
	}
	return registeredAccounts;
}

Account * AccountList::firstRegisteredAccount() const
{
	if(firstAccount != NULL)
	{
		return firstAccount;
	}
	Account * current;
	for (int i = 0; i < accounts->count(); ++i){
		current = (*accounts)[i];
		if(current->getAccountDetail(ACCOUNT_STATUS) == QString(ACCOUNT_STATE_REGISTERED))
		{
			return current;
		}
	}
	return NULL;
}

void AccountList::setAccountFirst(Account * account)
{
	firstAccount = account;
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
	qDebug() << "for " << accounts->size();
	for (int i = 0; i < accounts->size(); ++i)
	{
		qDebug() << "account " << i << " (*accounts)[i]->getAccountId() " << (*accounts)[i];
		if (!(*accounts)[i]->isNew() && (*accounts)[i]->getAccountId() == id)
		{
			qDebug() << "found ";
			return (*accounts)[i];
		}
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

const Account & AccountList::operator[] (int i) const
{
	return *((*accounts)[i]);
}

Account & AccountList::operator[] (int i)
{
	return *((*accounts)[i]);
}
