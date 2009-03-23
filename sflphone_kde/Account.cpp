#include "Account.h"
#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"

#include <iostream>

using namespace std;

const QString account_state_name(QString & s)
{
	if(s == QString(ACCOUNT_STATE_REGISTERED))
		return QApplication::translate("ConfigurationDialog", "Registered", 0, QApplication::UnicodeUTF8);
	if(s == QString(ACCOUNT_STATE_UNREGISTERED))
		return QApplication::translate("ConfigurationDialog", "Not Registered", 0, QApplication::UnicodeUTF8);
	if(s == QString(ACCOUNT_STATE_TRYING))
		return QApplication::translate("ConfigurationDialog", "Trying...", 0, QApplication::UnicodeUTF8);
	if(s == QString(ACCOUNT_STATE_ERROR))
		return QApplication::translate("ConfigurationDialog", "Error", 0, QApplication::UnicodeUTF8);
	if(s == QString(ACCOUNT_STATE_ERROR_AUTH))
		return QApplication::translate("ConfigurationDialog", "Bad authentification", 0, QApplication::UnicodeUTF8);
	if(s == QString(ACCOUNT_STATE_ERROR_NETWORK))
		return QApplication::translate("ConfigurationDialog", "Network unreachable", 0, QApplication::UnicodeUTF8);
	if(s == QString(ACCOUNT_STATE_ERROR_HOST))
		return QApplication::translate("ConfigurationDialog", "Host unreachable", 0, QApplication::UnicodeUTF8);
	if(s == QString(ACCOUNT_STATE_ERROR_CONF_STUN))
		return QApplication::translate("ConfigurationDialog", "Stun configuration error", 0, QApplication::UnicodeUTF8);
	if(s == QString(ACCOUNT_STATE_ERROR_EXIST_STUN))
		return QApplication::translate("ConfigurationDialog", "Stun server invalid", 0, QApplication::UnicodeUTF8);
	return QApplication::translate("ConfigurationDialog", "Invalid", 0, QApplication::UnicodeUTF8);
}

//Constructors

	Account::Account():accountId(NULL){}

/*
Account::Account(QListWidgetItem & _item, QString & alias)
{
	accountDetails = new MapStringString();
	(*accountDetails)[ACCOUNT_ALIAS] = alias;
	item = & _item;
}

Account::Account(QString & _accountId, MapStringString & _accountDetails, account_state_t & _state)
{
	*accountDetails = _accountDetails;
	*accountId = _accountId;
	*state = _state;
}
*/

void Account::initAccountItem()
{
	item = new QListWidgetItem();
	item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled|Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
	cout << getAccountDetail(*(new QString(ACCOUNT_ENABLED))).toStdString() << endl;
	item->setCheckState((getAccountDetail(*(new QString(ACCOUNT_ENABLED))) == ACCOUNT_ENABLED_TRUE) ? Qt::Checked : Qt::Unchecked);
	item->setText(getAccountDetail(*(new QString(ACCOUNT_ALIAS))));
}

Account * Account::buildExistingAccountFromId(QString _accountId)
{
	Account * a = new Account();
	a->accountId = new QString(_accountId);
	a->accountDetails = new MapStringString( ConfigurationManagerInterfaceSingleton::getInstance().getAccountDetails(_accountId).value() );
	a->initAccountItem();
	if(a->item->checkState() == Qt::Checked)
	{
		if(a->getAccountDetail(* new QString(ACCOUNT_STATUS)) == ACCOUNT_STATE_REGISTERED)
			a->item->setTextColor(Qt::darkGreen);
		else
			a->item->setTextColor(Qt::red);
	}
	return a;
}

Account * Account::buildNewAccountFromAlias(QString alias)
{
	Account * a = new Account();
	a->accountDetails = new MapStringString();
	a->setAccountDetail(QString(ACCOUNT_ALIAS),alias);
	a->initAccountItem();
	return a;
}

Account::~Account()
{
	delete accountId;
	delete accountDetails;
	delete item;
}

//Getters

bool Account::isNew()
{
	qDebug() << accountId;
	return(!accountId);
}

QString & Account::getAccountId()
{
	return *accountId; 
}

MapStringString & Account::getAccountDetails()
{
	return *accountDetails;
}

QListWidgetItem * Account::getItem()
{
	if(!item)
		cout<<"null"<<endl;
	return item;
	
}

QString Account::getStateName(QString & state)
{
	return account_state_name(state);
}

QColor Account::getStateColor()
{
	if(item->checkState() == Qt::Checked)
	{
		if(getAccountDetail(* new QString(ACCOUNT_STATUS)) == ACCOUNT_STATE_REGISTERED)
			return Qt::darkGreen;
		return Qt::red;
	}
	return Qt::black;
}


QString Account::getStateColorName()
{
	if(item->checkState() == Qt::Checked)
	{
		if(getAccountDetail(* new QString(ACCOUNT_STATUS)) == ACCOUNT_STATE_REGISTERED)
			return "darkGreen";
		return "red";
	}
	return "black";
}

QString Account::getAccountDetail(QString & param)
{
	return (*accountDetails)[param];
}
/*
QString Account::getAccountDetail(std::string param)
{
	return (*accountDetails)[QString(param)];
}
*/
//Setters
/*
void Account::setAccountId(QString id)
{
	accountId = id;
}
*/
void Account::setAccountDetails(MapStringString m)
{
	*accountDetails = m;
}
/*
void Account::setState(account_state_t s)
{
	
}
*/
void Account::setAccountDetail(QString param, QString val)
{
	(*accountDetails)[param] = val;
}

//Operators
bool Account::operator==(const Account& a)const
{
	return *accountId == *a.accountId;
}
