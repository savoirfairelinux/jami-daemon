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

#include "ConfigAccountList.h"

//KDE
#include <KDebug>

#include "lib/sflphone_const.h"
#include "lib/configurationmanager_interface_singleton.h"

///Constructor
ConfigAccountList::ConfigAccountList(QStringList &_accountIds) : QObject()
{
   accounts = new QVector<AccountView*>();
   for (int i = 0; i < _accountIds.size(); ++i) {
      (*accounts) += AccountView::buildExistingAccountFromId(_accountIds[i]);
   }
}

///Constructor
///@param fill Keep the list empty (false), load all account (true)
ConfigAccountList::ConfigAccountList(bool fill) : QObject()
{
   accounts = new QVector<AccountView*>();
   if(fill)
      updateAccounts();
}

///Destructor
ConfigAccountList::~ConfigAccountList()
{
   foreach(Account* a, *accounts) {
      delete a;
   }
   delete accounts;
}

///Get an account using a widget
AccountView* ConfigAccountList::getAccountByItem(QListWidgetItem* item)
{
   for (int i = 0; i < accounts->size(); ++i) {
      if ((*accounts)[i]->getItem() == item)
         return (*accounts)[i];
   }
   return NULL;
}

///Add an account
AccountView* ConfigAccountList::addAccount(const QString& alias)
{
   AccountView* a = AccountView::buildNewAccountFromAlias(alias);
   (*accounts) += a;
   return a;
}

///Remove an account
void ConfigAccountList::removeAccount(QListWidgetItem* item)
{
   if(!item) {
      kDebug() << "Attempting to remove an account from a NULL item.";
      return;
   }

   AccountView* a = (AccountView*) getAccountByItem(item);
   if(!a) {
      kDebug() << "Attempting to remove an unexisting account.";
      return;
   }

   accounts->remove(accounts->indexOf(a));
}

///Operator overload to access an account using its position in the list
AccountView* ConfigAccountList::operator[] (int i)
{
   if (i < accounts->size())
      return (*accounts)[i];
   else
      return 0;
}

///Remove an account
void ConfigAccountList::removeAccount(AccountView* account)
{
   accounts->remove(accounts->indexOf(account));
}

///Get an account by id
AccountView* ConfigAccountList::getAccountById(const QString & id) const
{
   if(id.isEmpty())
          return NULL;
   for (int i = 0; i < accounts->size(); ++i) {
      if (!(*accounts)[i]->isNew() && (*accounts)[i]->getAccountId() == id)
         return (*accounts)[i];
   }
   return NULL;
}

///Get an account according to its state
QVector<AccountView*> ConfigAccountList::getAccountByState(QString & state)
{
   QVector<AccountView*> v;
   for (int i = 0; i < accounts->size(); ++i) {
      if ((*accounts)[i]->getAccountRegistrationStatus() == state)
         v += (*accounts)[i];
   }
   return v;
}

///Return the list of all loaded accounts
QVector<AccountView*>& ConfigAccountList::getAccounts()
{
   return *accounts;
}

///Get account at index 'i'
const AccountView* ConfigAccountList::getAccountAt(int i) const
{
   return (*accounts)[i];
}

///Get account at index 'i'
AccountView* ConfigAccountList::getAccountAt (int i)
{
   return (*accounts)[i];
}

///Update the list
void ConfigAccountList::update()
{
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   AccountView* current;
   for (int i = 0; i < accounts->size(); i++) {
      current = (*accounts)[i];
      if (!(*accounts)[i]->isNew())
         removeAccount(current);
   }
   //ask for the list of accounts ids to the configurationManager
   QStringList accountIds = configurationManager.getAccountList().value();
   for (int i = 0; i < accountIds.size(); ++i) {
      accounts->insert(i, AccountView::buildExistingAccountFromId(accountIds[i]));
   }
}

///Reload accounts
void ConfigAccountList::updateAccounts()
{
   kDebug() << "updateAccounts";
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   QStringList accountIds = configurationManager.getAccountList().value();
   accounts->clear();
   for (int i = 0; i < accountIds.size(); ++i) {
      (*accounts) += AccountView::buildExistingAccountFromId(accountIds[i]);
   }
   emit accountListUpdated();
}

///Move account up
void ConfigAccountList::upAccount(int index)
{
   if(index <= 0 || index >= size()) {
      kDebug() << "Error : index or future index out of range in upAccount.";
      return;
   }
   AccountView* account = getAccountAt(index);
   accounts->remove(index);
   accounts->insert(index - 1, account);
}

///Move account down
void ConfigAccountList::downAccount(int index)
{
   if(index < 0 || index >= size() - 1) {
      kDebug() << "Error : index or future index out of range in upAccount.";
      return;
   }
   AccountView* account = getAccountAt(index);
   accounts->remove(index);
   accounts->insert(index + 1, account);
}

///Get an account list separated by '/'
QString ConfigAccountList::getOrderedList() const
{
   QString order;
   for( int i = 0 ; i < size() ; i++) {
      order += getAccountAt(i)->getAccountId() + "/";
   }
   return order;
}

///Return a list of all registered accounts
QVector<AccountView*> ConfigAccountList::registeredAccounts() const
{
   QVector<AccountView*> registeredAccounts;
   AccountView* current;
   for (int i = 0; i < accounts->count(); ++i) {
      current = (*accounts)[i];
      if(current->getAccountRegistrationStatus() == ACCOUNT_STATE_REGISTERED) {
         kDebug() << current->getAlias() << " : " << current;
         registeredAccounts.append(current);
      }
   }
   return registeredAccounts;
}

///Return the first registered account
AccountView* ConfigAccountList::firstRegisteredAccount() const
{
   AccountView* current;
   for (int i = 0; i < accounts->count(); ++i) {
      current = (*accounts)[i];
      if(current->getAccountRegistrationStatus() == ACCOUNT_STATE_REGISTERED)
      {
         return current;
      }
   }
   return NULL;
}

///Return the number (count) of accounts
int ConfigAccountList::size() const
{
   return accounts->size();
}

