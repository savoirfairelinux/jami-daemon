// /***************************************************************************
//  *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
//  *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
//  *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
//  *                                                                         *
//  *   This program is free software; you can redistribute it and/or modify  *
//  *   it under the terms of the GNU General Public License as published by  *
//  *   the Free Software Foundation; either version 3 of the License, or     *
//  *   (at your option) any later version.                                   *
//  *                                                                         *
//  *   This program is distributed in the hope that it will be useful,       *
//  *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//  *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
//  *   GNU General Public License for more details.                          *
//  *                                                                         *
//  *   You should have received a copy of the GNU General Public License     *
//  *   along with this program; if not, write to the                         *
//  *   Free Software Foundation, Inc.,                                       *
//  *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
//  **************************************************************************/
// 
// //Parent
// #include "AccountListModel.h"
// 
// //SFLPhone library
// #include "lib/sflphone_const.h"
// 
// //SFLPhone
// #include "conf/ConfigAccountList.h"
// 
// //Qt
// #include <QtGui/QIcon>
// 
// //KDE
// #include <KLed>
// 
// ///Constructor
// AccountListModel::AccountListModel(QObject *parent)
//  : QAbstractListModel(parent),accounts(NULL)
// {
//    //this->accounts = new ConfigAccountList();
// }
// 
// ///Destructor
// AccountListModel::~AccountListModel()
// {
//    if (accounts) delete accounts;
// }
// 
// 
// /*****************************************************************************
//  *                                                                           *
//  *                                  Getters                                  *
//  *                                                                           *
//  ****************************************************************************/
// 
// 
// 
// 
// /*****************************************************************************
//  *                                                                           *
//  *                                  Setters                                  *
//  *                                                                           *
//  ****************************************************************************/
// 
// 
// 
// /*****************************************************************************
//  *                                                                           *
//  *                                  Mutator                                  *
//  *                                                                           *
//  ****************************************************************************/
// 
// 
// ///Remove an account
// bool AccountListModel::removeAccount( int index )
// {
//    if(index >= 0 && index < rowCount()) {
//       accounts->removeAccount(accounts->getAccountAt(index));
//       emit dataChanged(this->index(index, 0, QModelIndex()), this->index(rowCount(), 0, QModelIndex()));
//       return true;
//    }
//    return false;
// }
// 
// ///Add an account
// bool AccountListModel::addAccount(const QString& alias )
// {
//    accounts->addAccount(alias);
//    emit dataChanged(this->index(0, 0, QModelIndex()), this->index(rowCount(), 0, QModelIndex()));
//    return true;
// }