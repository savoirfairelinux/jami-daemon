#include "Account.h"
#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"
#include "kled.h"

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
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	item = new QListWidgetItem();
	item->setSizeHint(QSize(140,25));
	//item->setTextAlignment(Qt::AlignCenter);
	item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled|Qt::ItemIsEnabled);
	bool enabled = getAccountDetail(*(new QString(ACCOUNT_ENABLED))) == ACCOUNT_ENABLED_TRUE;
	//item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
	setItemText(getAccountDetail(*(new QString(ACCOUNT_ALIAS))));
	itemWidget = new QWidget();
	QCheckBox * checkbox = new QCheckBox(itemWidget);
	checkbox->setObjectName("checkbox");
	checkbox->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
	//QLabel* name = new QLabel(getAccountDetail(*(new QString(ACCOUNT_ALIAS))), itemWidget);
	//QLabel* name = new QLabel("", itemWidget);
	KLed * led = new KLed(itemWidget);
	led->setObjectName("led");
	led->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
	if(! isNew() && enabled)
	{
		led->setState(KLed::On);
		if(getAccountDetail(* new QString(ACCOUNT_STATUS)) == ACCOUNT_STATE_REGISTERED)
		{
			led->setColor(QColor(0,255,0));
		}
		else
		{
			led->setColor(QColor(255,0,0));
		}
	}
	else
	{
		led->setState(KLed::Off);
	}
	QHBoxLayout* hlayout = new QHBoxLayout();
	hlayout->setContentsMargins(0,0,0,0);
	hlayout->addWidget(checkbox);
	//hlayout->addWidget(name);
	hlayout->addWidget(led);
	itemWidget->setLayoutDirection(Qt::LeftToRight);
	itemWidget->setLayout(hlayout);
	//itemWidget->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	//item->parent()->setItemWidget(item, widget);
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

QWidget * Account::getItemWidget()
{
	if(!item)
		cout<<"null"<<endl;
	return itemWidget;
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
