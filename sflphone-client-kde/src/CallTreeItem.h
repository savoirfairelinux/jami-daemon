/***************************************************************************
 *   Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).     *
 *   All rights reserved.                                                  *
 *   Contact: Nokia Corporation (qt-info@nokia.com)                        *
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
 ***************************************************************************/

/**
 * http://doc.trolltech.com/4.5/itemviews-editabletreemodel.html
 */

#ifndef CALLTREE_ITEM_H
#define CALLTREE_ITEM_H

#include <QtCore/QList>
#include <QtCore/QVariant>
#include <QtCore/QVector>

#include <QtGui/QWidget>
#include <QtGui/QLabel>
#include <QtGui/QSpacerItem>
#include <QtGui/QHBoxLayout>
#include <QtGui/QVBoxLayout>

#include "Call.h"

class CallTreeItem : QObject
 {
 public:
	 CallTreeItem(const QVector<QVariant> &data, CallTreeItem *parent);
	 ~CallTreeItem();
     
	 CallTreeItem *child(int number);
	 int childCount() const;
	 int columnCount() const;
	 QVariant data(int column) const;
	 Call* call() const;
	 QWidget* widget() const;
	 bool insertChildren(int position, int count, int columns);
	 bool insertColumns(int position, int columns);
	 CallTreeItem *parent();
	 bool removeChildren(int position, int count);
	 bool removeColumns(int position, int columns);
	 int childNumber() const;
	 bool setData(int column, const QVariant &value);
	 void setCall(Call *call);
	 void updateItem();

	static const char * callStateIcons[11];
 private:
	 QList<CallTreeItem*> childItems;
	 QVector<QVariant> itemData;
	 CallTreeItem *parentItem;
	 Call *itemCall;

	 QWidget *itemWidget;

	 QLabel * labelIcon;
	 QLabel * labelPeerName;
	 QLabel * labelCallNumber;
	 QLabel * labelTransferPrefix;
	 QLabel * labelTransferNumber;
	 
	 QWidget * historyItemWidget;
	 QLabel * labelHistoryIcon;
	 QLabel * labelHistoryPeerName;
	 QLabel * labelHistoryCallNumber;
	 QLabel * labelHistoryTime;
 };

#endif // CALLTREE_ITEM_H
