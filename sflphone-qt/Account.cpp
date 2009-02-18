#include "Account.h"
#include "sflphone_const.h"
#include "daemon_interface_singleton.h"

const QString account_state_name(account_state_t & s)
{
  QString state;
  switch(s)
  {
  case ACCOUNT_STATE_REGISTERED:
    state = QApplication::translate("ConfigurationDialog", "Registered", 0, QApplication::UnicodeUTF8);
    break;
  case ACCOUNT_STATE_UNREGISTERED:
    state = QApplication::translate("ConfigurationDialog", "Not Registered", 0, QApplication::UnicodeUTF8);
    break;
  case ACCOUNT_STATE_TRYING:
    state = QApplication::translate("ConfigurationDialog", "Trying...", 0, QApplication::UnicodeUTF8);
    break;
  case ACCOUNT_STATE_ERROR:
    state = QApplication::translate("ConfigurationDialog", "Error", 0, QApplication::UnicodeUTF8);
    break;
  case ACCOUNT_STATE_ERROR_AUTH:
    state = QApplication::translate("ConfigurationDialog", "Bad authentification", 0, QApplication::UnicodeUTF8);
    break;
  case ACCOUNT_STATE_ERROR_NETWORK:
    state = QApplication::translate("ConfigurationDialog", "Network unreachable", 0, QApplication::UnicodeUTF8);
    break;
  case ACCOUNT_STATE_ERROR_HOST:
    state = QApplication::translate("ConfigurationDialog", "Host unreachable", 0, QApplication::UnicodeUTF8);
    break;
  case ACCOUNT_STATE_ERROR_CONF_STUN:
    state = QApplication::translate("ConfigurationDialog", "Stun configuration error", 0, QApplication::UnicodeUTF8);
    break;
  case ACCOUNT_STATE_ERROR_EXIST_STUN:
    state = QApplication::translate("ConfigurationDialog", "Stun server invalid", 0, QApplication::UnicodeUTF8);
    break;
  default:
    state = QApplication::translate("ConfigurationDialog", "Invalid", 0, QApplication::UnicodeUTF8);
    break;
  }
  return state;
}

//Constructors
Account::Account(QListWidgetItem & _item, QString & alias)
{
	accountDetails = new MapStringString();
	(*accountDetails)[ACCOUNT_ALIAS] = alias;
	item = & _item;
}
/*
Account::Account(QString & _accountId, MapStringString & _accountDetails, account_state_t & _state)
{
	*accountDetails = _accountDetails;
	*accountId = _accountId;
	*state = _state;
}
*/
Account::Account(QString & _accountId)
{
	accountDetails = & DaemonInterfaceSingleton::getInstance().getAccountDetails(_accountId).value();
}

Account::~Account()
{
	delete accountId;
	delete accountDetails;
	delete state;
	delete item;
}

//Getters
QString & Account::getAccountId()
{
	return *accountId; 
}

MapStringString & Account::getAccountDetails()
{
	return *accountDetails;
}

account_state_t & Account::getState()
{
	return *state;
}

QListWidgetItem & Account::getItem()
{
	return *item;
}

QString Account::getStateName()
{
	return account_state_name(*state);
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
