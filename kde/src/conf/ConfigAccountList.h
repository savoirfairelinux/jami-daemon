#ifndef CONFIG_ACCOUNT_LIST_H
#define CONFIG_ACCOUNT_LIST_H

#include "../lib/AccountList.h"
#include "../AccountView.h"

class ConfigAccountList : public QObject {
   Q_OBJECT
   public:
      ConfigAccountList(bool fill = true);
      ConfigAccountList(QStringList &_accountIds);
      virtual AccountView* addAccount(QString & alias);
      AccountView* getAccountByItem(QListWidgetItem* item);
      void removeAccount(QListWidgetItem* item);
      void removeAccount(AccountView* account);
      AccountView* operator[] (int i);
      //const AccountView* operator[] (int i) const;
      QVector<AccountView*>&  getAccounts();
      const AccountView*  getAccountAt (int i) const;
      AccountView* getAccountAt (int i);
      QVector<AccountView*> getAccountByState(QString & state);
      AccountView* getAccountById(const QString & id) const;
      void update();
      void updateAccounts();
      void upAccount(int index);
      void downAccount(int index);
      QString getOrderedList() const;
      QVector<AccountView*> registeredAccounts() const;
      AccountView* firstRegisteredAccount() const;
      int size() const;

   private:
      QVector<AccountView*>*  accounts;
      
   signals:
      void accountListUpdated();
};

#endif
