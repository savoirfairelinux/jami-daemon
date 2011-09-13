/***************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                              *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
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
#include <QDebug>

#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"


const QString account_state_name(QString & s)
{
   if(s == QString(ACCOUNT_STATE_REGISTERED))
      return "Registered";
   if(s == QString(ACCOUNT_STATE_UNREGISTERED))
      return "Not Registered";
   if(s == QString(ACCOUNT_STATE_TRYING))
      return "Trying...";
   if(s == QString(ACCOUNT_STATE_ERROR))
      return "Error";
   if(s == QString(ACCOUNT_STATE_ERROR_AUTH))
      return "Authentication Failed";
   if(s == QString(ACCOUNT_STATE_ERROR_NETWORK))
      return "Network unreachable";
   if(s == QString(ACCOUNT_STATE_ERROR_HOST))
      return "Host unreachable";
   if(s == QString(ACCOUNT_STATE_ERROR_CONF_STUN))
      return "Stun configuration error";
   if(s == QString(ACCOUNT_STATE_ERROR_EXIST_STUN))
      return "Stun server invalid";
   return "Invalid";
}

//Constructors

Account::Account():accountId(NULL),accountDetails(NULL)
{
}
#include <unistd.h>
Account* Account::buildExistingAccountFromId(QString _accountId)
{
   qDebug() << "Building an account from id: " << _accountId;
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   Account* a = new Account();
   a->accountId = new QString(_accountId);
   MapStringString* aDetails = new MapStringString(configurationManager.getAccountDetails(_accountId).value());
   //MapStringString* accountDetails = &configurationManager.getAccountDetails(_accountId).value(); //SegFault???
   
   if (!aDetails->count()) {
      qDebug() << "Account not found";
      return NULL;
   }
   a->accountDetails = aDetails;
   return a;
}

Account* Account::buildNewAccountFromAlias(QString alias)
{
   qDebug() << "Building an account from alias: " << alias;
   Account* a = new Account();
   a->accountDetails = new MapStringString();
   a->setAccountDetail(ACCOUNT_ALIAS,alias);
   return a;
}

Account::~Account()
{
   delete accountId;
   //delete accountDetails;
   //delete item;
}

//Getters

bool Account::isNew() const
{
   return (accountId == NULL);
}

const QString & Account::getAccountId() const
{
   if (isNew())
      qDebug() << "Error : getting AccountId of a new account.";
   if (!accountId) {
      qDebug() << "Account not configured";
      return ""; //WARNING May explode
   }
   
   return *accountId; 
}

MapStringString& Account::getAccountDetails() const
{
   return *accountDetails;
}

QString Account::getStateName(QString & state)
{
   return account_state_name(state);
}

QString Account::getAccountDetail(QString param) const
{
   if (!accountDetails) {
      qDebug() << "The account list is not set";
      return NULL; //May crash, but better than crashing now
   }
   if (accountDetails->find(param) != accountDetails->end())
      return (*accountDetails)[param];
   else {
      qDebug() << "Account details not found, there is " << accountDetails->count() << " details available";
      return NULL;
   }
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
      qDebug() << "Error : setting AccountId of an existing account.";
   accountId = new QString(id);
}

void Account::setEnabled(bool checked)
{
   setAccountDetail(ACCOUNT_ENABLED, checked ? ACCOUNT_ENABLED_TRUE : ACCOUNT_ENABLED_FALSE);
}

bool Account::isEnabled() const
{
   qDebug() << "isEnabled = " << getAccountDetail(ACCOUNT_ENABLED);
   return (getAccountDetail(ACCOUNT_ENABLED) == ACCOUNT_ENABLED_TRUE);
}

bool Account::isRegistered() const
{
   qDebug() << "isRegistered = " << getAccountDetail(ACCOUNT_STATUS);
   return (getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED);
}

void Account::updateState()
{
   qDebug() << "updateState";
   if(! isNew()) {
      ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
      MapStringString details = configurationManager.getAccountDetails(getAccountId()).value();
      QString status = details[ACCOUNT_STATUS];
      setAccountDetail(ACCOUNT_STATUS, status); //Update -internal- object state
   }
}

//Operators
bool Account::operator==(const Account& a)const
{
   return *accountId == *a.accountId;
}


