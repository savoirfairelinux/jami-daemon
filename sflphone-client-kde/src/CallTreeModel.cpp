/***************************************************************************
 *   Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).     *
 *   All rights reserved.                                                  *
 *   Contact: Nokia Corporation (qt-info@nokia.com)                        *
 *   Author : Mathieu Leduc-Hamel mathieu.leduc-hamel@savoirfairelinux.com *
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
 ***************************************************************************/

#include <QtGui>
#include <klocale.h>

#include "CallTreeModel.h"
#include "CallTreeItem.h"
#include "calllist_interface_singleton.h"

CallTreeModel::CallTreeModel(QObject *parent)
   : QAbstractItemModel(parent),
     rootItem(0)
     
{
   QStringList data = QString("Calls").split("\n");
   QVector<QVariant> rootData;
   rootData << i18n("Calls");

   rootItem = new CallTreeItem(rootData, 0);
   setupModelData(data, rootItem);
}

CallTreeModel::~CallTreeModel()
{
   if(rootItem) {
      delete rootItem;
   }
}

int CallTreeModel::columnCount(const QModelIndex & /* parent */) const
{
   return rootItem->columnCount();
}

QVariant CallTreeModel::data(const QModelIndex &index, int role) const
{
   if (!index.isValid()) {
      return QVariant();
   }

   if (role != Qt::DisplayRole && role != Qt::EditRole) {
      return QVariant();
   }

   CallTreeItem *item = getItem(index);

   return item->data(index.column());
}

Call* CallTreeModel::call(const QModelIndex &index, int role) const
{
   if (!index.isValid()) {
      return 0;
   }

   if (role != Qt::DisplayRole && role != Qt::EditRole) {
      return 0;
   }

   CallTreeItem *item = getItem(index);

   return item->call();
}

Qt::ItemFlags CallTreeModel::flags(const QModelIndex &index) const
{
   if (!index.isValid()) {
      return 0;
   }
   Qt::ItemFlags val = Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

   return val;
}

CallTreeItem *CallTreeModel::getItem(const QModelIndex &index) const
{
   if (index.isValid()) {
      CallTreeItem *item = static_cast<CallTreeItem*>(index.internalPointer());
      if (item) {
         return item;
      }
   }
   return rootItem;
}

QVariant CallTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
      return rootItem->data(section);
   }
   return QVariant();
}

QModelIndex CallTreeModel::index(int row, int column, const QModelIndex &parent) const
{
   if (parent.isValid() && parent.column() != 0) {
      return QModelIndex();
   }

   CallTreeItem *parentItem = getItem(parent);
   CallTreeItem *childItem = parentItem->child(row);

   if (childItem) {
      return createIndex(row, column, childItem);
   }
   else {
      return QModelIndex();
   }
}

bool CallTreeModel::insertColumns(int position, int columns, const QModelIndex &parent)
{
   bool success;

   beginInsertColumns(parent, position, position + columns - 1);
   success = rootItem->insertColumns(position, columns);
   endInsertColumns();

   return success;
}

bool CallTreeModel::insertRows(int position, int rows, const QModelIndex &parent)
{
   CallTreeItem *parentItem = getItem(parent);
   bool success;

   beginInsertRows(parent, position, position + rows - 1);
   success = parentItem->insertChildren(position, rows, rootItem->columnCount());
   endInsertRows();

   return success;
}

QModelIndex CallTreeModel::parent(const QModelIndex &index) const
{
   if (!index.isValid()) {
      return QModelIndex();
   }

   CallTreeItem *childItem = getItem(index);
   CallTreeItem *parentItem = childItem->parent();

   if (parentItem == rootItem) {
      return QModelIndex();
   }

   return createIndex(parentItem->childNumber(), 0, parentItem);
}

bool CallTreeModel::removeColumns(int position, int columns, const QModelIndex &parent)
{
   bool success;

   beginRemoveColumns(parent, position, position + columns - 1);
   success = rootItem->removeColumns(position, columns);
   endRemoveColumns();

   if (rootItem->columnCount() == 0) {
      removeRows(0, rowCount());
   }

   return success;
}

bool CallTreeModel::removeRows(int position, int rows, const QModelIndex &parent)
{
   CallTreeItem *parentItem = getItem(parent);
   bool success = true;

   beginRemoveRows(parent, position, position + rows - 1);
   success = parentItem->removeChildren(position, rows);
   endRemoveRows();

   return success;
}

int CallTreeModel::rowCount(const QModelIndex &parent) const
{
   CallTreeItem *parentItem = getItem(parent);

   return parentItem->childCount();
}

bool CallTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
   if (role != Qt::EditRole) {
      return false;
   }

   CallTreeItem *item = getItem(index);
   bool result = item->setData(index.column(), value);
        
        //item->setData(1, QString("test"));
        //item->setData(2, QString("test2"));

   if (result) {
      emit dataChanged(index, index);
   }

   return result;
}

bool CallTreeModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
   if (role != Qt::EditRole || orientation != Qt::Horizontal) {
      return false;
   }

   bool result = rootItem->setData(section, value);

   if (result) {
      emit headerDataChanged(orientation, section, section);
   }

   return result;
}

void CallTreeModel::setupModelData(const QStringList &lines, CallTreeItem *parent)
{
   QList<CallTreeItem*> parents;
   QList<int> indentations;
//   parents << parent;
   indentations << 0;

   int number = 0;

   while (number < lines.count()) {
      int position = 0;

      while (position < lines[number].length()) {
         if (lines[number].mid(position, 1) != " ") {
            break;
         }
         position++;
      }

      QString lineData = lines[number].mid(position).trimmed();

      if (!lineData.isEmpty()) {
         // Read the column data from the rest of the line.
         QStringList columnStrings = lineData.split("\t", QString::SkipEmptyParts);
         QVector<QVariant> columnData;
         for (int column = 0; column < columnStrings.count(); ++column) {
            columnData << columnStrings[column];
         }

         if (position > indentations.last()) {
            // The last child of the current parent is now the new parent
            // unless the current parent has no children.

            if (parents.last()->childCount() > 0) {
               parents << parents.last()->child(parents.last()->childCount()-1);
               indentations << position;
            }
         } 
         else {
            while (position < indentations.last() && parents.count() > 0) {
               parents.pop_back();
               indentations.pop_back();
            }
         }

         // Append a new item to the current parent's list of children.
         /*CallTreeItem *parent = parents.last();
         parent->insertChildren(parent->childCount(), 1, rootItem->columnCount());
         
         for (int column = 0; column < columnData.size(); ++column)
         {
            parent->child(parent->childCount() - 1)->setData(column, columnData[column]);
            }*/
      }      
      number++;
   }
}

Qt::DropActions CallTreeModel::supportedDropActions() 
{
  return Qt::MoveAction;// | Qt::CopyAction ;
}

bool CallTreeModel::dropMimeData( const QMimeData * data, Qt::DropAction action, int row, int column, const QModelIndex & parent ) {  
  
  if (action == Qt::IgnoreAction)
      return true;

  if (!data->hasFormat("application/vnd.text.list"))
      return false;

  if (column > 0)
      return false;
  
  int beginRow;

  if (row != -1)
      beginRow = row;
  else if (parent.isValid())
      beginRow = parent.row();
  else
      beginRow = rowCount(QModelIndex());
  
  CallTreeItem *item = getItem(parent);
  QByteArray encodedData = data->data("application/vnd.text.list");
  QDataStream stream(&encodedData, QIODevice::ReadOnly);
  QStringList newItems;
  int rows = 0;

  while (!stream.atEnd()) {
      QString text;
      stream >> text;
      newItems << text;
      ++rows;
  }
  
  if (rows) {
    Call* secondCall = CallListInterfaceSingleton::getInstance().findCallByCallId(newItems[0]);
    Call* conVersation = CallListInterfaceSingleton::getInstance().createConversationFromCall(item->call(), secondCall);
    emit joinCall(newItems[0], item->call()->getCallId());
    
     insertRows(beginRow, rows, parent);
     foreach (QString text, newItems) {
         QModelIndex idx = index(beginRow, 0, parent);
         setData(idx, "test");
         beginRow++;
     }
  }
  else
    qDebug() << "Unknow drop";

  return true;
}

//This can be modified later to implement an universal drag and drop
QStringList CallTreeModel::mimeTypes() const
{
    QStringList types;
    types << "application/vnd.text.list";
    return types;
}

QMimeData* CallTreeModel::mimeData(const QModelIndexList &indexes) const
{
     QMimeData *mimeData = new QMimeData();
     QByteArray encodedData;
     QDataStream stream(&encodedData, QIODevice::WriteOnly);

     foreach (QModelIndex index, indexes) {
         if (index.isValid()) {
             CallTreeItem *item = getItem(index);
             stream << item->call()->getCallId();
         }
     }

     mimeData->setData("application/vnd.text.list", encodedData);
     return mimeData;
}