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

#ifndef ACCOUNT_LIST_H
#define ACCOUNT_LIST_H


#include <QtCore/QVector>

#include "Account.h"
#include "typedefs.h"

///AccountList: List of all daemon accounts
class LIB_EXPORT AccountList : public QObject{
   Q_OBJECT

public:
   //Static getter and destructor
   static AccountList* getInstance();
   static void destroy();
   
   //Getters
   const QVector<Account*>& getAccounts            (                        );
   QVector<Account*>        getAccountsByState     ( const QString& state   );
   QString                  getOrderedList         (                        ) const;
   Account*                 getAccountById         ( const QString& id      ) const;
   Account*                 getAccountAt           ( int i                  );
   const Account*           getAccountAt           ( int i                  ) const;
   int                      size                   (                        ) const;
   Account*                 firstRegisteredAccount (                        ) const;
   static Account*          getCurrentAccount      (                        );
   static QString           getPriorAccoundId      (                        );

   //Setters
   static void setPriorAccountId(const QString& value );
   
   //Mutators
   virtual Account*  addAccount        ( QString & alias  )      ;
   void              removeAccount     ( Account* account )      ;
   QVector<Account*> registeredAccounts(                  ) const;

   //Operators
   Account*       operator[] (int i)      ;
   const Account* operator[] (int i) const;
   
private:
   //Constructors & Destructors
   AccountList(QStringList & _accountIds);
   AccountList(bool fill = true);
   ~AccountList();
   
   //Attributes
   QVector<Account*>*  m_pAccounts;
   static AccountList* m_spAccountList;
   static QString      m_sPriorAccountId;
   
public slots:
   void update();
   void updateAccounts();
   
signals:
   ///The account list changed
   void accountListUpdated();
};

#endif
