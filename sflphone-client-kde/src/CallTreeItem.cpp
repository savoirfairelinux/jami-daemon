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

#include <QStringList>

 #include "CallTreeItem.h"

 CallTreeItem::CallTreeItem(const Call &data, CallTreeItem *parent)
 {
     parentItem = parent;
     itemData = data;
 }

 CallTreeItem::~CallTreeItem()
 {
     qDeleteAll(childItems);
 }

 CallTreeItem *CallTreeItem::child(int number)
 {
     return childItems.value(number);
 }

 int CallTreeItem::childCount() const
 {
     return childItems.count();
 }

 int CallTreeItem::childNumber() const
 {
     if (parentItem)
     {
         return parentItem->childItems.indexOf(const_cast<CallTreeItem*>(this));
     }
     return 0;
 }

 int CallTreeItem::columnCount() const
 {
     return itemData.count();
 }

 QVariant CallTreeItem::data(int column) const
 {
     return itemData.value(column);
 }

 bool CallTreeItem::insertChildren(int position, int count, int columns)
 {
     if (position < 0 || position > childItems.size())
     {
         return false;
     }

     for (int row = 0; row < count; ++row) 
     {
         QVector<QVariant> data(columns);
         CallTreeItem *item = new CallTreeItem(data, this);
         childItems.insert(position, item);
     }

     return true;
 }

 bool CallTreeItem::insertColumns(int position, int columns)
 {
     if (position < 0 || position > itemData.size())
     {
         return false;
     }

     for (int column = 0; column < columns; ++column)
     {
         itemData.insert(position, QVariant());
     }

     foreach (CallTreeItem *child, childItems)
     {
         child->insertColumns(position, columns);
     }

     return true;
 }

 CallTreeItem *CallTreeItem::parent()
 {
     return parentItem;
 }

 bool CallTreeItem::removeChildren(int position, int count)
 {
     if (position < 0 || position + count > childItems.size())
     {
         return false;
     }

     for (int row = 0; row < count; ++row)
     {
         delete childItems.takeAt(position);
     }

     return true;
 }

 bool CallTreeItem::removeColumns(int position, int columns)
 {
     if (position < 0 || position + columns > itemData.size())
     {
         return false;
     }

     for (int column = 0; column < columns; ++column)
     {
         itemData.remove(position);
     }

     foreach (CallTreeItem *child, childItems)
     {
         child->removeColumns(position, columns);
     }

     return true;
 }

 bool CallTreeItem::setData(int column, const QVariant &value)
 {
     if (column < 0 || column >= itemData.size())
     {
         return false;
     }

     itemData[column] = value;
     return true;
 }
