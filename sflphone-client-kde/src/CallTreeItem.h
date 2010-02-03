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
#include <QtGui/QTreeWidget>

class Call;

class CallTreeItem : public QTreeWidget
 {
 public:
	 CallTreeItem(CallTreeItem *parent = 0);
	 CallTreeItem(const Call &data, CallTreeItem *parent = 0);
	 ~CallTreeItem();
     
	 CallTreeItem *child(int number);
	 int childCount() const;
	 int columnCount() const;
	 Call* data(int column) const;
	 Call* data() const;
	 bool insertChildren(int position, int count, int columns);
	 bool insertColumns(int position, int columns);
	 CallTreeItem *parent();
	 bool removeChildren(int position, int count);
	 bool removeColumns(int position, int columns);
	 int childNumber() const;
	 bool setData(int column, const Call &value);

 private:
	 QList<CallTreeItem*> childItems;
	 Call *itemData;
	 CallTreeItem *parentItem;
 };

#endif // CALLTREE_ITEM_H
