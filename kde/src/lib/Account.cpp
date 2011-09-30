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
#include <QtCore/QString>
#include <QtGui/QColor>

#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"

///Match state name to user readable string
const QString account_state_name(QString & s)
{
   if(s == QString(ACCOUNT_STATE_REGISTERED)       )
      return "Registered"               ;
   if(s == QString(ACCOUNT_STATE_UNREGISTERED)     )
      return "Not Registered"           ;
   if(s == QString(ACCOUNT_STATE_TRYING)           )
      return "Trying..."                ;
   if(s == QString(ACCOUNT_STATE_ERROR)            )
      return "Error"                    ;
   if(s == QString(ACCOUNT_STATE_ERROR_AUTH)       )
      return "Authentication Failed"    ;
   if(s == QString(ACCOUNT_STATE_ERROR_NETWORK)    )
      return "Network unreachable"      ;
   if(s == QString(ACCOUNT_STATE_ERROR_HOST)       )
      return "Host unreachable"         ;
   if(s == QString(ACCOUNT_STATE_ERROR_CONF_STUN)  )
      return "Stun configuration error" ;
   if(s == QString(ACCOUNT_STATE_ERROR_EXIST_STUN) )
      return "Stun server invalid"      ;
   return "Invalid"                     ;
}

///Constructors
Account::Account():m_pAccountId(NULL),m_pAccountDetails(NULL)
{
}

///Build an account from it'id
Account* Account::buildExistingAccountFromId(QString _accountId)
{
   qDebug() << "Building an account from id: " << _accountId;
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   Account* a = new Account();
   a->m_pAccountId = new QString(_accountId);
   MapStringString* aDetails = new MapStringString(configurationManager.getAccountDetails(_accountId).value());
   
   if (!aDetails->count()) {
      qDebug() << "Account not found";
      return NULL;
   }
   a->m_pAccountDetails = aDetails;
   return a;
}

///Build an account from it's name / alias
Account* Account::buildNewAccountFromAlias(QString alias)
{
   qDebug() << "Building an account from alias: " << alias;
   Account* a = new Account();
   a->m_pAccountDetails = new MapStringString();
   a->setAccountDetail(ACCOUNT_ALIAS,alias);
   return a;
}

///Destructor
Account::~Account()
{
   delete m_pAccountId;
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///IS this account new
bool Account::isNew() const
{
   return (m_pAccountId == NULL);
}

///Get this account ID
const QString & Account::getAccountId() const
{
   if (isNew())
      qDebug() << "Error : getting AccountId of a new account.";
   if (!m_pAccountId) {
      qDebug() << "Account not configured";
      return ""; //WARNING May explode
   }
   
   return *m_pAccountId; 
}

///Get this account details
MapStringString& Account::getAccountDetails() const
{
   return *m_pAccountDetails;
}

///Get current state
QString Account::getStateName(QString & state)
{
   return account_state_name(state);
}

///Get an account detail
QString Account::getAccountDetail(QString param) const
{
   if (!m_pAccountDetails) {
      qDebug() << "The account list is not set";
      return NULL; //May crash, but better than crashing now
   }
   if (m_pAccountDetails->find(param) != m_pAccountDetails->end())
      return (*m_pAccountDetails)[param];
   else {
      qDebug() << "Account details not found, there is " << m_pAccountDetails->count() << " details available";
      return NULL;
   }
}

///Get the alias
QString Account::getAlias() const
{
   return getAccountDetail(ACCOUNT_ALIAS);
}

///Is this account enabled
bool Account::isEnabled() const
{
   qDebug() << "isEnabled = " << getAccountDetail(ACCOUNT_ENABLED);
   return (getAccountDetail(ACCOUNT_ENABLED) == ACCOUNT_ENABLED_TRUE);
}

///Is this account registered
bool Account::isRegistered() const
{
   qDebug() << "isRegistered = " << getAccountDetail(ACCOUNT_STATUS);
   return (getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED);
}


/*****************************************************************************
 *                                                                           *
 *                                  Setters                                  *
 *                                                                           *
 ****************************************************************************/

///Set account details
void Account::setAccountDetails(MapStringString m)
{
   *m_pAccountDetails = m;
}

///Set a specific detail
void Account::setAccountDetail(QString param, QString val)
{
   (*m_pAccountDetails)[param] = val;
}

///Set the account id
void Account::setAccountId(QString id)
{
   qDebug() << "accountId = " << m_pAccountId;
   if (! isNew())
      qDebug() << "Error : setting AccountId of an existing account.";
   m_pAccountId = new QString(id);
}

///Set account enabled
void Account::setEnabled(bool checked)
{
   setAccountDetail(ACCOUNT_ENABLED, checked ? ACCOUNT_ENABLED_TRUE : ACCOUNT_ENABLED_FALSE);
}

/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Update the account
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

/*****************************************************************************
 *                                                                           *
 *                                 Operator                                  *
 *                                                                           *
 ****************************************************************************/

///Are both account the same
bool Account::operator==(const Account& a)const
{
   return *m_pAccountId == *a.m_pAccountId;
}


