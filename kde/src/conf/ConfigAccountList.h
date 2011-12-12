#ifndef CONFIG_ACCOUNT_LIST_H
#define CONFIG_ACCOUNT_LIST_H

#include "../lib/AccountList.h"
#include "../AccountView.h"

class ConfigAccountList : public QObject {
   Q_OBJECT
   public:

      ///Constructor
      ConfigAccountList(bool fill = true);
      ConfigAccountList(QStringList &_accountIds);

      ///Getters
      const AccountView*      getAccountAt           ( int i                 ) const;
      AccountView*            getAccountAt           ( int i                 )      ;
      QVector<AccountView*>&  getAccounts            (                       )      ;
      AccountView*            firstRegisteredAccount (                       ) const;
      QVector<AccountView*>   registeredAccounts     (                       ) const;
      QString                 getOrderedList         (                       ) const;
      int                     size                   (                       ) const;
      AccountView*            getAccountByItem       ( QListWidgetItem* item )      ;
      QVector<AccountView*>   getAccountByState      ( QString & state       )      ;
      AccountView*            getAccountById         ( const QString & id    ) const;
      
      ///Mutators
      virtual AccountView* addAccount     ( QString & alias       );
      void                 removeAccount  ( QListWidgetItem* item );
      void                 removeAccount  ( AccountView* account  );
      void                 update         (                       );
      void                 updateAccounts (                       );
      void                 upAccount      ( int index             );
      void                 downAccount    ( int index             );

      ///Operators
      AccountView* operator[] (int i);

   private:
      QVector<AccountView*>*  accounts;
      
   signals:
      void accountListUpdated();
};

#endif
