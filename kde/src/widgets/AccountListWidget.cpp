// /***************************************************************************
//  *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
//  *   Author : Emmanuel Lepage Valle <emmanuel.lepage@savoirfairelinux.com >*
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
// #include "AccountListWidget.h"
// 
// #include <QtCore/QDebug>
// 
// #include "../lib/Account.h"
// #include "../lib/AccountList.h"
// 
// void AccountListWidget::currentChanged( const QModelIndex & current, const QModelIndex & previous )
// {
//    emit currentIndexChanged(current,previous);
//    itemChanged_private();
// }
// 
// 
// void AccountListWidget::dataChanged ( const QModelIndex & topLeft, const QModelIndex & bottomRight )
// {
//    itemChanged_private();
//    QListView::dataChanged(topLeft,bottomRight);
// }
// 
// void AccountListWidget::itemChanged_private()
// {
//    Account* newCurrent = AccountList::getInstance()->getAccountByModelIndex(currentIndex());
//    if (newCurrent != m_pCurrentAccount)
//       emit currentAccountChanged(newCurrent,m_pCurrentAccount);
//    m_pCurrentAccount = newCurrent;
// }