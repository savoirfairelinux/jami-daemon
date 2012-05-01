/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
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
 **************************************************************************/

#ifndef ACCOUNT_LIST_H
#define ACCOUNT_LIST_H


#include <QtCore/QVector>

#include "Account.h"
#include "typedefs.h"

///@class AccountList List of all daemon accounts
class LIB_EXPORT AccountList : public QObject{
   Q_OBJECT

public:

   //Constructors & Destructors
   AccountList(QStringList & _accountIds);
   AccountList(bool fill = true);
   ~AccountList();
   
   //Getters
   const QVector<Account*>& getAccounts            (                        );
   QVector<Account*>        getAccountsByState     ( const QString& state   );
   QString                  getOrderedList         (                        ) const;
   Account*                 getAccountById         ( const QString& id      ) const;
   Account*                 getAccountAt           ( int i                  );
   const Account*           getAccountAt           ( int i                  ) const;
   int                      size                   (                        ) const;
   Account*                 firstRegisteredAccount (                        ) const;
   
   //Mutators
   virtual Account*  addAccount        ( QString & alias  )      ;
   void              removeAccount     ( Account* account )      ;
   QVector<Account*> registeredAccounts(                  ) const;

   //Operators
   Account*       operator[] (int i)      ;
   const Account* operator[] (int i) const;
   
private:
   //Attributes
   QVector<Account*>*  m_pAccounts;
   
public slots:
   void update();
   void updateAccounts();
   
signals:
   void accountListUpdated();
};

#endif
