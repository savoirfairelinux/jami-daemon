/***************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                              *
 *   Author : Jérémy Quentin                                               *
 *   jeremy.quentin@savoirfairelinux.com                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "Account.h"

#include <QtGui/QApplication>
#include <klocale.h>

#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"


const QString account_state_name(QString & s)
{
	if(s == QString(ACCOUNT_STATE_REGISTERED))
		return i18nc("account state", "Registered" );
	if(s == QString(ACCOUNT_STATE_UNREGISTERED))
		return i18nc("account state", "Not Registered");
	if(s == QString(ACCOUNT_STATE_TRYING))
		return i18nc("account state", "Trying...");
	if(s == QString(ACCOUNT_STATE_ERROR))
		return i18nc("account state", "Error");
	if(s == QString(ACCOUNT_STATE_ERROR_AUTH))
		return i18nc("account state", "Authentication Failed");
	if(s == QString(ACCOUNT_STATE_ERROR_NETWORK))
		return i18nc("account state", "Network unreachable");
	if(s == QString(ACCOUNT_STATE_ERROR_HOST))
		return i18nc("account state", "Host unreachable");
	if(s == QString(ACCOUNT_STATE_ERROR_CONF_STUN))
		return i18nc("account state", "Stun configuration error");
	if(s == QString(ACCOUNT_STATE_ERROR_EXIST_STUN))
		return i18nc("account state", "Stun server invalid");
	return i18nc("account state", "Invalid");
}

//Constructors

Account::Account():accountId(NULL)
{
}


void Account::initItem()
{
	if(item != NULL)
	{
		delete item;
	}
	item = new QListWidgetItem();
	item->setSizeHint(QSize(140,25));
	item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled|Qt::ItemIsEnabled);
	initItemWidget();
}

void Account::initItemWidget()
{
	if(itemWidget != NULL)
	{
		delete itemWidget;
	}
	bool enabled = getAccountDetail(ACCOUNT_ENABLED) == ACCOUNT_ENABLED_TRUE;
	itemWidget = new AccountItemWidget();
	itemWidget->setEnabled(enabled);
	itemWidget->setAccountText(getAccountDetail(ACCOUNT_ALIAS));
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
	connect(itemWidget, SIGNAL(checkStateChanged(bool)), this, SLOT(setEnabled(bool)));
}

Account * Account::buildExistingAccountFromId(QString _accountId)
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	Account * a = new Account();
	a->accountId = new QString(_accountId);
	a->accountDetails = new MapStringString( configurationManager.getAccountDetails(_accountId).value() );
	a->initItem();
	return a;
}

Account * Account::buildNewAccountFromAlias(QString alias)
{
	Account * a = new Account();
	a->accountDetails = new MapStringString();
	a->setAccountDetail(ACCOUNT_ALIAS,alias);
	a->initItem();
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

const QString & Account::getAccountId() const
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
	return item;
}

AccountItemWidget * Account::getItemWidget()
{
	return itemWidget;
}

QString Account::getStateName(QString & state)
{
	return account_state_name(state);
}

QColor Account::getStateColor()
{
	if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_UNREGISTERED)
	{	return Qt::black;	}
	if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED)
	{	return Qt::darkGreen;	}
	return Qt::red;
}


QString Account::getStateColorName()
{
	if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_UNREGISTERED)
	{	return "black";	}
	if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED)
	{	return "darkGreen";	}
	return "red";
}

QString Account::getAccountDetail(QString param) const
{
	return (*accountDetails)[param];
}

QString Account::getAlias() const
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
	qDebug() << "accountId = " << accountId;
	if (! isNew())
	{
		qDebug() << "Error : setting AccountId of an existing account.";
	}
	accountId = new QString(id);
}

void Account::setEnabled(bool checked)
{
	qDebug() << "setEnabled = " << checked;
	setAccountDetail(ACCOUNT_ENABLED, checked ? ACCOUNT_ENABLED_TRUE : ACCOUNT_ENABLED_FALSE);
}

bool Account::isEnabled() const
{
	return (getAccountDetail(ACCOUNT_ENABLED) == ACCOUNT_ENABLED_TRUE);
}

bool Account::isRegistered() const
{
	return (getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED);
}

void Account::updateState()
{
	qDebug() << "updateState";
	if(! isNew())
	{
		ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
		MapStringString details = configurationManager.getAccountDetails(getAccountId()).value();
		AccountItemWidget * itemWidget = getItemWidget();
		QString status = details[ACCOUNT_STATUS];
		setAccountDetail(ACCOUNT_STATUS, status);
		if(getAccountDetail(ACCOUNT_ENABLED) != ACCOUNT_ENABLED_TRUE )
		{
			qDebug() << "itemWidget->setState(AccountItemWidget::Unregistered);";
			itemWidget->setState(AccountItemWidget::Unregistered);
		}
		else if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED)
		{
			qDebug() << "itemWidget->setState(AccountItemWidget::Registered);";
			itemWidget->setState(AccountItemWidget::Registered);
		}
		else
		{
			qDebug() << "itemWidget->setState(AccountItemWidget::NotWorking);";
			itemWidget->setState(AccountItemWidget::NotWorking);
		}
	}
}

//Operators
bool Account::operator==(const Account& a)const
{
	return *accountId == *a.accountId;
}
