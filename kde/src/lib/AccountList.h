/****************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                               *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#ifndef ACCOUNT_LIST_H
#define ACCOUNT_LIST_H


#include <QtCore/QVector>
#include <QtCore/QAbstractListModel>

#include "Account.h"
#include "typedefs.h"
#include "dbus/metatypes.h"

class AccountListColorVisitor;

///AccountList: List of all daemon accounts
class LIB_EXPORT AccountList : public QAbstractListModel {
   Q_OBJECT

public:
   friend class Account;
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
   Account*                 getDefaultAccount      (                        );
   static Account*          getCurrentAccount      (                        );
   static QString           getPriorAccoundId      (                        );

   //Getters
   QVariant      data     ( const QModelIndex& index, int role = Qt::DisplayRole ) const;
   int           rowCount ( const QModelIndex& parent = QModelIndex()            ) const;
   Qt::ItemFlags flags    ( const QModelIndex& index                             ) const;
   Account*      getAccountByModelIndex(QModelIndex item)                          const;
   void          setDefaultAccount(Account* a);

   //Setters
   static  void setPriorAccountId( const QString& value                                     );
   virtual bool setData          ( const QModelIndex& index, const QVariant &value, int role);
   void         setColorVisitor  ( AccountListColorVisitor* visitor                         );
   
   //Mutators
   virtual Account*  addAccount        ( QString & alias   )      ;
   void              removeAccount     ( Account* account  )      ;
   void              removeAccount     ( QModelIndex index )      ;
   QVector<Account*> registeredAccounts(                   ) const;
   void              save              (                   )      ;
   bool              accountUp         ( int index         )      ;
   bool              accountDown       ( int index         )      ;

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
   Account*            m_pDefaultAccount;
   AccountListColorVisitor* m_pColorVisitor;
   
public slots:
   void update();
   void updateAccounts();

private slots:
   void accountChanged(const QString& account,const QString& state, int code);
   void accountChanged(Account* a);
   
signals:
   ///The account list changed
   void accountListUpdated();
   ///Emitted when an account state change
   void accountStateChanged( Account* account, QString state);
   void accountEnabledChanged(Account* source);
   void defaultAccountChanged(Account* a);
};

///SFLPhonelib Qt does not link to QtGui, and does not need to, this allow to add runtime Gui support
class LIB_EXPORT AccountListColorVisitor {
public:
   virtual QVariant getColor(const Account* a) = 0;
   virtual QVariant getIcon(const Account* a)  = 0;
   virtual ~AccountListColorVisitor() {}
};

#endif
