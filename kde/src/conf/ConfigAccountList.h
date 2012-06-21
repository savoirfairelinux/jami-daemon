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

#ifndef CONFIG_ACCOUNT_LIST_H
#define CONFIG_ACCOUNT_LIST_H

#include "../lib/AccountList.h"
#include "../AccountView.h"

///ConfigAccountList: Account list model
class ConfigAccountList : public QObject {
   Q_OBJECT
   public:

      ///Constructor
      ConfigAccountList(bool fill = true);
      ConfigAccountList(QStringList &_accountIds);
      ~ConfigAccountList();

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
      virtual AccountView* addAccount     ( const QString & alias );
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
      ///Emitted when the list change
      void accountListUpdated();
};

#endif
