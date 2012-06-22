/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/

//Parent
#include "AccountList.h"

//SFLPhone
#include "sflphone_const.h"

//SFLPhone library
#include "configurationmanager_interface_singleton.h"
#include "callmanager_interface_singleton.h"

AccountList* AccountList::m_spAccountList   = nullptr;
QString      AccountList::m_sPriorAccountId = ""     ;

///Constructors
AccountList::AccountList(QStringList & _accountIds) : m_pColorVisitor(nullptr)
{
   m_pAccounts = new QVector<Account*>();
   for (int i = 0; i < _accountIds.size(); ++i) {
      (*m_pAccounts) += Account::buildExistingAccountFromId(_accountIds[i]);
      emit dataChanged(index(size()-1,0),index(size()-1,0));
   }
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   connect(&callManager, SIGNAL(registrationStateChanged(QString,QString,int)),this,SLOT(accountChanged(QString,QString,int)));
}

///Constructors
///@param fill Whether to fill the list with accounts from configurationManager or not.
AccountList::AccountList(bool fill) : m_pColorVisitor(nullptr)
{
   m_pAccounts = new QVector<Account *>();
   if(fill)
      updateAccounts();
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   connect(&callManager, SIGNAL(registrationStateChanged(QString,QString,int)),this,SLOT(accountChanged(QString,QString,int)));
}

///Destructor
AccountList::~AccountList()
{
   foreach(Account* a,*m_pAccounts) {
      delete a;
   }
   delete m_pAccounts;
}

///Singleton
AccountList* AccountList::getInstance()
{
   if (not m_spAccountList) {
      m_spAccountList = new AccountList(true);
   }
   return m_spAccountList;
}

///Static destructor
void AccountList::destroy()
{
   if (m_spAccountList)
      delete m_spAccountList;
   m_spAccountList = nullptr;
}

///Account status changed
void AccountList::accountChanged(const QString& account,const QString& state, int code)
{
   Q_UNUSED(code)
   Account* a = AccountList::getInstance()->getAccountById(account);
   if (!a) {
      ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
      QStringList accountIds = configurationManager.getAccountList().value();
      m_pAccounts->clear();
      for (int i = 0; i < accountIds.size(); ++i) {
         if (!getAccountById(accountIds[i])) {
            m_pAccounts->insert(i, Account::buildExistingAccountFromId(accountIds[i]));
            emit dataChanged(index(i,0),index(size()-1));
         }
      }
      a = AccountList::getInstance()->getAccountById(account);
   }
   if (a)
      emit accountStateChanged(a,a->getStateName(state));
   else
      qDebug() << "Account not found";
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Update accounts
void AccountList::update()
{
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   Account* current;
   for (int i = 0; i < m_pAccounts->size(); i++) {
      current = (*m_pAccounts)[i];
      if (!(*m_pAccounts)[i]->isNew())
         removeAccount(current);
   }
   //ask for the list of accounts ids to the configurationManager
   QStringList accountIds = configurationManager.getAccountList().value();
   for (int i = 0; i < accountIds.size(); ++i) {
      m_pAccounts->insert(i, Account::buildExistingAccountFromId(accountIds[i]));
      emit dataChanged(index(i,0),index(size()-1,0));
   }
} //update

///Update accounts
void AccountList::updateAccounts()
{
   qDebug() << "updateAccounts";
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   QStringList accountIds = configurationManager.getAccountList().value();
   m_pAccounts->clear();
   for (int i = 0; i < accountIds.size(); ++i) {
      qDebug() << "updateAccounts " << accountIds[i];
      (*m_pAccounts) += Account::buildExistingAccountFromId(accountIds[i]);
      emit dataChanged(index(size()-1,0),index(size()-1,0));
   }
   emit accountListUpdated();
} //updateAccounts

///Save accounts details and reload it
void AccountList::save()
{
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   QStringList accountIds= QStringList(configurationManager.getAccountList().value());

   //create or update each account from accountList
   for (int i = 0; i < size(); i++) {
      Account* current = (*this)[i];
      QString currentId;
      current->save();
      currentId = QString(current->getAccountId());
   }

   //remove accounts that are in the configurationManager but not in the client
   for (int i = 0; i < accountIds.size(); i++) {
      if(!getAccountById(accountIds[i])) {
         configurationManager.removeAccount(accountIds[i]);
      }
   }

   configurationManager.setAccountsOrder(getOrderedList());
}

///Move account up
bool AccountList::accountUp( int index )
{
   if(index > 0 && index <= rowCount()) {
      Account* account = getAccountAt(index);
      m_pAccounts->remove(index);
      m_pAccounts->insert(index - 1, account);
      emit dataChanged(this->index(index - 1, 0, QModelIndex()), this->index(index, 0, QModelIndex()));
      return true;
   }
   return false;
}

///Move account down
bool AccountList::accountDown( int index )
{
   if(index >= 0 && index < rowCount()) {
      Account* account = getAccountAt(index);
      m_pAccounts->remove(index);
      m_pAccounts->insert(index + 1, account);
      emit dataChanged(this->index(index, 0, QModelIndex()), this->index(index + 1, 0, QModelIndex()));
      return true;
   }
   return false;
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///Get all accounts
const QVector<Account*>& AccountList::getAccounts()
{
   return *m_pAccounts;
}

///Get a single account
const Account* AccountList::getAccountAt (int i) const
{
   return (*m_pAccounts)[i];
}

///Get a single account
Account* AccountList::getAccountAt (int i)
{
   return (*m_pAccounts)[i];
}

///Get a serialized string of all accounts
QString AccountList::getOrderedList() const
{
   QString order;
   for( int i = 0 ; i < size() ; i++) {
      order += getAccountAt(i)->getAccountId() + "/";
   }
   return order;
}

///Get account using its ID
Account* AccountList::getAccountById(const QString & id) const
{
   if(id.isEmpty())
          return NULL;
   for (int i = 0; i < m_pAccounts->size(); ++i) {
      if (!(*m_pAccounts)[i]->isNew() && (*m_pAccounts)[i]->getAccountId() == id)
         return (*m_pAccounts)[i];
   }
   return NULL;
}

///Get account with a specific state
QVector<Account*> AccountList::getAccountsByState(const QString& state)
{
   QVector<Account *> v;
   for (int i = 0; i < m_pAccounts->size(); ++i) {
      if ((*m_pAccounts)[i]->getAccountRegistrationStatus() == state)
         v += (*m_pAccounts)[i];
   }
   return v;
}

///Get a list of all registerred account
QVector<Account*> AccountList::registeredAccounts() const
{
   qDebug() << "registeredAccounts";
   QVector<Account*> registeredAccounts;
   Account* current;
   for (int i = 0; i < m_pAccounts->count(); ++i) {
      current = (*m_pAccounts)[i];
      if(current->getAccountRegistrationStatus() == ACCOUNT_STATE_REGISTERED) {
         qDebug() << current->getAlias() << " : " << current;
         registeredAccounts.append(current);
      }
   }
   return registeredAccounts;
}

///Get the first registerred account (default account)
Account* AccountList::firstRegisteredAccount() const
{
   Account* current;
   for (int i = 0; i < m_pAccounts->count(); ++i) {
      current = (*m_pAccounts)[i];
      if(current && current->getAccountRegistrationStatus() == ACCOUNT_STATE_REGISTERED && current->isAccountEnabled())
         return current;
      else if (current && (current->getAccountRegistrationStatus() == ACCOUNT_STATE_READY) && m_pAccounts->count() == 1)
         return current;
      else if (current && !(current->getAccountRegistrationStatus() == ACCOUNT_STATE_READY)) {
         qDebug() << "Account " << ((current)?current->getAccountId():"") << " is not registered ("
         << ((current)?current->getAccountRegistrationStatus():"") << ") State:"
         << ((current)?current->getAccountRegistrationStatus():"");
      }
   }
   return NULL;
}

///Get the account size
int AccountList::size() const
{
   return m_pAccounts->size();
}

///Return the current account
Account* AccountList::getCurrentAccount()
{
   Account* priorAccount = AccountList::getInstance()->getAccountById(m_sPriorAccountId);
   if(priorAccount && priorAccount->getAccountDetail(ACCOUNT_REGISTRATION_STATUS) == ACCOUNT_STATE_REGISTERED && priorAccount->isAccountEnabled() ) {
      return priorAccount;
   }
   else {
      Account* a = AccountList::getInstance()->firstRegisteredAccount();
      if (a)
         return AccountList::getInstance()->firstRegisteredAccount();
      else
         return AccountList::getInstance()->getAccountById("IP2IP");
   }
} //getCurrentAccount

///Return the previously used account ID
QString AccountList::getPriorAccoundId()
{
   return m_sPriorAccountId;
}

///Get data from the model
QVariant AccountList::data ( const QModelIndex& index, int role) const
{
   if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
      return QVariant();

   const Account * account = (*m_pAccounts)[index.row()];
   if(index.column() == 0 && role == Qt::DisplayRole)
      return QVariant(account->getAlias());
   else if(index.column() == 0 && role == Qt::CheckStateRole)
      return QVariant(account->isEnabled() ? Qt::Checked : Qt::Unchecked);
   else if (role == Qt::BackgroundRole) {
      if (m_pColorVisitor)
         return m_pColorVisitor->getColor(account);
      else
         return QVariant(account->getStateColor());
   }
   else if(index.column() == 0 && role == Qt::DecorationRole) {
      /*TODO implement visitor*/
   }
   return QVariant();
} //data

///Flags for "index"
Qt::ItemFlags AccountList::flags(const QModelIndex & index) const
{
   if (index.column() == 0)
      return QAbstractItemModel::flags(index) | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
   return QAbstractItemModel::flags(index);
}

///Number of account
int AccountList::rowCount(const QModelIndex & /*parent*/) const
{
   return m_pAccounts->size();
}

Account* AccountList::getAccountByModelIndex(QModelIndex item) const {
   return (*m_pAccounts)[item.row()];
}


/*****************************************************************************
 *                                                                           *
 *                                  Setters                                  *
 *                                                                           *
 ****************************************************************************/

///Add an account
Account* AccountList::addAccount(QString & alias)
{
   Account* a = Account::buildNewAccountFromAlias(alias);
   (*m_pAccounts) += a;
   
   emit dataChanged(index(m_pAccounts->size()-1,0), index(m_pAccounts->size()-1,0));
   return a;
}

///Remove an account
void AccountList::removeAccount(Account* account)
{
   if (not account) return;
   qDebug() << "Removing" << m_pAccounts;
   int aindex = m_pAccounts->indexOf(account);
   m_pAccounts->remove(aindex);
   emit dataChanged(index(aindex,0), index(m_pAccounts->size()-1,0));
}

void AccountList::removeAccount( QModelIndex index )
{
   removeAccount(getAccountByModelIndex(index));
}

///Set the previous account used
void AccountList::setPriorAccountId(const QString& value) {
   m_sPriorAccountId = value;
}

///Set model data
bool AccountList::setData(const QModelIndex & index, const QVariant &value, int role)
{
   if (index.isValid() && index.column() == 0 && role == Qt::CheckStateRole) {
      (*m_pAccounts)[index.row()]->setEnabled(value.toBool());
      emit dataChanged(index, index);
      return true;
   }
   emit dataChanged(index, index);
   return false;
}

///Set QAbstractItemModel BackgroundRole visitor
void AccountList::setColorVisitor(AccountListColorVisitor* visitor)
{
   m_pColorVisitor = visitor;
}


/*****************************************************************************
 *                                                                           *
 *                                 Operator                                  *
 *                                                                           *
 ****************************************************************************/

///Get the account from its index
const Account* AccountList::operator[] (int i) const
{
   return (*m_pAccounts)[i];
}

///Get the account from its index
Account* AccountList::operator[] (int i)
{
   return (*m_pAccounts)[i];
}
