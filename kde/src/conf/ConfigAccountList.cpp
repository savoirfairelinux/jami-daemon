#include "ConfigAccountList.h"

//KDE
#include <KDebug>

#include "lib/sflphone_const.h"
#include "lib/configurationmanager_interface_singleton.h"

ConfigAccountList::ConfigAccountList(QStringList &_accountIds) : QObject()
{
   accounts = new QVector<AccountView*>();
   for (int i = 0; i < _accountIds.size(); ++i) {
      (*accounts) += AccountView::buildExistingAccountFromId(_accountIds[i]);
   }
}

ConfigAccountList::ConfigAccountList(bool fill) : QObject()
{
   accounts = new QVector<AccountView*>();
   if(fill)
      updateAccounts();
}

AccountView* ConfigAccountList::getAccountByItem(QListWidgetItem * item)
{
   for (int i = 0; i < accounts->size(); ++i) {
      if ((*accounts)[i]->getItem() == item)
         return (*accounts)[i];
   }
   return NULL;
}

AccountView* ConfigAccountList::addAccount(const QString& alias)
{
   AccountView* a = AccountView::buildNewAccountFromAlias(alias);
   (*accounts) += a;
   return a;
}

void ConfigAccountList::removeAccount(QListWidgetItem* item)
{
   if(!item)
      kDebug() << "Attempting to remove an account from a NULL item."; return;

   AccountView* a = (AccountView*) getAccountByItem(item);
   if(!a)
      kDebug() << "Attempting to remove an unexisting account."; return;

   accounts->remove(accounts->indexOf(a));
}

AccountView* ConfigAccountList::operator[] (int i)
{
   if (i < accounts->size())
      return (*accounts)[i];
   else
      return 0;
}

void ConfigAccountList::removeAccount(AccountView* account)
{
   accounts->remove(accounts->indexOf(account));
}



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

QVector<AccountView*> ConfigAccountList::getAccountByState(QString & state)
{
   QVector<AccountView*> v;
   for (int i = 0; i < accounts->size(); ++i) {
      if ((*accounts)[i]->getAccountDetail(REGISTRATION_STATUS) == state)
         v += (*accounts)[i];
   }
   return v;
}


QVector<AccountView*>& ConfigAccountList::getAccounts()
{
   return *accounts;
}

const AccountView* ConfigAccountList::getAccountAt(int i) const
{
   return (*accounts)[i];
}

AccountView* ConfigAccountList::getAccountAt (int i)
{
   return (*accounts)[i];
}

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


QString ConfigAccountList::getOrderedList() const
{
   QString order;
   for( int i = 0 ; i < size() ; i++) {
      order += getAccountAt(i)->getAccountId() + "/";
   }
   return order;
}

QVector<AccountView*> ConfigAccountList::registeredAccounts() const
{
   QVector<AccountView*> registeredAccounts;
   AccountView* current;
   for (int i = 0; i < accounts->count(); ++i) {
      current = (*accounts)[i];
      if(current->getAccountDetail(REGISTRATION_STATUS) == ACCOUNT_STATE_REGISTERED) {
         kDebug() << current->getAlias() << " : " << current;
         registeredAccounts.append(current);
      }
   }
   return registeredAccounts;
}

AccountView* ConfigAccountList::firstRegisteredAccount() const
{
   AccountView* current;
   for (int i = 0; i < accounts->count(); ++i) {
      current = (*accounts)[i];
      if(current->getAccountDetail(REGISTRATION_STATUS) == ACCOUNT_STATE_REGISTERED)
      {
         return current;
      }
   }
   return NULL;
}

int ConfigAccountList::size() const
{
   return accounts->size();
}

