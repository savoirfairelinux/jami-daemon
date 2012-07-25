/***************************************************************************
 *   Copyright (C) 2012 by Savoir-Faire Linux                              *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
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
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

#ifndef IM_TAB_H
#define IM_TAB_H

#include <QtGui/QListView>
#include <QtGui/QStyledItemDelegate>

class InstantMessagingModel;
class IMTab;

class ImDelegates : public QStyledItemDelegate
{
   Q_OBJECT
public:
   explicit ImDelegates(IMTab* parent = nullptr);
protected:
   virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
   virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const;
private:
   IMTab* m_pParent;
};

class IMTab : public QListView
{
   Q_OBJECT
public:
   explicit IMTab(InstantMessagingModel* model,QWidget* parent = nullptr);
private slots:
   void scrollBottom();
};

#endif // IM_MANAGER
