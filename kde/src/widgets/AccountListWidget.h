/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
 *   Author : Emmanuel Lepage Valle <emmanuel.lepage@savoirfairelinux.com >*
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
#ifndef ACCOUNT_LIST_WIDGET_H
#define ACCOUNT_LIST_WIDGET_H

#include <QtGui/QListView>

//Qt
class QModelIndex;

//SFLPhone
class Account;

class AccountListWidget : public QListView {
   Q_OBJECT
public:
   AccountListWidget(QWidget* parent = nullptr):QListView(parent),m_pCurrentAccount(nullptr){}
protected:
   void currentChanged ( const QModelIndex & current, const QModelIndex & previous );
   void dataChanged ( const QModelIndex & topLeft, const QModelIndex & bottomRight );
private:
   Account* m_pCurrentAccount;

   void itemChanged_private();
signals:
   void currentIndexChanged(QModelIndex current, QModelIndex previous);
   void currentAccountChanged(Account* current, Account* previous);
};

#endif