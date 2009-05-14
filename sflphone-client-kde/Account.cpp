#include "Account.h"

#include <QtGui/QApplication>

#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"


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

/**
 * Sets text of the item associated with some spaces to avoid writing under checkbox.
 * @param text the text to set in the item
 */
void Account::setItemText(QString text)
{
	item->setText("       " + text);
}

void Account::initAccountItem()
{
	//ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	item = new QListWidgetItem();
	item->setSizeHint(QSize(140,25));
	item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled|Qt::ItemIsEnabled);
	bool enabled = getAccountDetail(ACCOUNT_ENABLED) == ACCOUNT_ENABLED_TRUE;
	setItemText(getAccountDetail(ACCOUNT_ALIAS));
	itemWidget = new AccountItemWidget();
	itemWidget->setEnabled(enabled);
	if(isNew() || !enabled)
	{
		itemWidget->setState(AccountItemWidget::Unregistered);
	}
	else if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED)
	{
		itemWidget->setState(AccountItemWidget::Registered);
	}
	else
	{
		itemWidget->setState(AccountItemWidget::NotWorking);
	}
}

Account * Account::buildExistingAccountFromId(QString _accountId)
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	Account * a = new Account();
	a->accountId = new QString(_accountId);
	a->accountDetails = new MapStringString( configurationManager.getAccountDetails(_accountId).value() );
	a->initAccountItem();
	return a;
}

Account * Account::buildNewAccountFromAlias(QString alias)
{
	Account * a = new Account();
	a->accountDetails = new MapStringString();
	a->setAccountDetail(ACCOUNT_ALIAS,alias);
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

bool Account::isNew() const
{
	return (accountId == NULL);
}

bool Account::isChecked() const
{
	return itemWidget->getEnabled();
}

QString & Account::getAccountId()
{
	if (isNew())
	{
		qDebug() << "Error : getting AccountId of a new account.";
	}
	return *accountId; 
}

MapStringString & Account::getAccountDetails() const
{
	return *accountDetails;
}

QListWidgetItem * Account::getItem()
{
	if(!item)
		qDebug() << "null" ;
	return item;
}

QListWidgetItem * Account::renewItem()
{
	if(!item)
		qDebug() << "null" ;
	item = new QListWidgetItem(*item);
	return item;
}

AccountItemWidget * Account::getItemWidget()
{
	delete itemWidget;
	bool enabled = getAccountDetail(ACCOUNT_ENABLED) == ACCOUNT_ENABLED_TRUE;
		itemWidget = new AccountItemWidget();
	itemWidget->setEnabled(enabled);
	if(isNew() || !enabled)
	{
		itemWidget->setState(AccountItemWidget::Unregistered);
	}
	else if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED)
	{
		itemWidget->setState(AccountItemWidget::Registered);
	}
	else
	{
		itemWidget->setState(AccountItemWidget::NotWorking);
	}
	return itemWidget;
}

QString Account::getStateName(QString & state)
{
	return account_state_name(state);
}

QColor Account::getStateColor()
{
	if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_UNREGISTERED)
		return Qt::black;
	if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED)
		return Qt::darkGreen;
	return Qt::red;
}


QString Account::getStateColorName()
{
	if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_UNREGISTERED)
		return "black";
	if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED)
		return "darkGreen";
	return "red";
}

QString Account::getAccountDetail(QString param) const
{
	return (*accountDetails)[param];
}

QString Account::getAlias()
{
	return getAccountDetail(ACCOUNT_ALIAS);
}


//Setters

void Account::setAccountDetails(MapStringString m)
{
	*accountDetails = m;
}

void Account::setAccountDetail(QString param, QString val)
{
	(*accountDetails)[param] = val;
}

void Account::setAccountId(QString id)
{
	if (! isNew())
	{
		qDebug() << "Error : setting AccountId of an existing account.";
	}
	*accountId = id;
}

//Operators
bool Account::operator==(const Account& a)const
{
	return *accountId == *a.accountId;
}
