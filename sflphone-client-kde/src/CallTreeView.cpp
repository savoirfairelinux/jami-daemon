/***************************************************************************
 *   Copyright (C) 2010 by Savoir-Faire Linux                              *
 *   Author : Mathieu Leduc-Hamel                                          *
 *   mathieu.leduc-hamel@savoirfairelinux.com                              *
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

#include <QtCore/QMimeData>

#include "CallTreeView.h"
#include "CallTreeModel.h"
#include "CallTreeItem.h"
#include "Call.h"
#include <QDebug>

CallTreeView::CallTreeView(QWidget * parent)
	: QTreeView(parent)
{	 
	treeModel = new CallTreeModel(this);
	setModel(treeModel);
        CallTreeItemDelegate *delegate = new CallTreeItemDelegate();
        setItemDelegate(delegate); 
	setHeaderHidden(true);
	setRootIsDecorated(false);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setDragEnabled(true);
	setAcceptDrops(true);
	setUniformRowHeights(true);
	setDropIndicatorShown(true);
        //setDragDropMode(QAbstractItemView::DragDrop);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
        
        setDragEnabled(TRUE);
        setAcceptDrops(TRUE);
        setDropIndicatorShown(TRUE);
        
        connect(this , SIGNAL(clicked(QModelIndex)), this, SLOT(itemClicked(QModelIndex)));
        connect(treeModel, SIGNAL(joinCall(QString,QString)), this, SLOT(joinCall(QString, QString)));
        connect(treeModel, SIGNAL(joinCall(QString,QString)), this, SLOT(expandAll()));
        connect(treeModel, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex& ) ), this, SLOT(adaptColumns(const QModelIndex &, const QModelIndex&) ) );
}

CallTreeView::~CallTreeView()
{
	delete treeModel;
}

CallTreeItem* CallTreeView::insert(Call *call)
{
	QModelIndex index = selectionModel()->currentIndex();
	int position = index.row()+1;
	QModelIndex parent = index.parent();
	CallTreeItem *item;

	if (!treeModel->insertRow(position, parent))
	{
		return 0;
	}
        
        QModelIndex child = model()->index(index.row()+1, 0, index.parent());
        treeModel->setData(child, QVariant(""), Qt::EditRole);
        
        for (int column = 1; column < treeModel->columnCount(index); ++column) 
        {
                QModelIndex child2 = treeModel->index(index.row()+1, column, index.parent());
                treeModel->setData(child2, QString("test"), Qt::EditRole);
        }
        
	item = treeModel->getItem(child);
 	item->setCall(call);
//         qDebug() << "Will connect, id " << call << ", " << call->getPeerPhoneNumber();
//         connect(call, SIGNAL(changed()), item, SLOT(updated()));
//         item->setCall(call);
//         item->setData(1,call->getPeerPhoneNumber());
//         item->setData(2,call->getPeerName());
//         resizeColumnToContents(0);
//         resizeColumnToContents(1);
//         resizeColumnToContents(2);
//         //item->updated();
	setIndexWidget(child, item->widget());
}

CallTreeItem* CallTreeView::insert(CallTreeItem *parent, Call *call)
{
	QModelIndex index = selectionModel()->currentIndex();

	if (treeModel->columnCount(index) == 0) 
	{
		if (!model()->insertColumn(0, index))
		{
			return 0; 
		}
			
	}
		
	if (!treeModel->insertRow(0, index))
	{
		return 0;
	}

	CallTreeItem *item = treeModel->getItem(index);
	
	for (int column = 0; column < treeModel->columnCount(index); ++column) 
	{
		QModelIndex child = treeModel->index(0, column, index);
                qDebug() << "I just added data: 0, " << column << " \n\n\n\n";
		treeModel->setData(child, QVariant(""), Qt::EditRole);      
	}

	item->setCall(call);
	selectionModel()->setCurrentIndex(model()->index(0, 0, index), QItemSelectionModel::ClearAndSelect);

	QModelIndex newIndex = selectionModel()->currentIndex();

	return treeModel->getItem(newIndex);
}

void CallTreeView::remove(QModelIndex & index) const
{
	treeModel->removeRow(index.row(), index.parent());
}

void CallTreeView::remove(Call* call) const //BUG not used
{
  for(int i=0; i < 15/* model.rowCount()*/;i++) { //TODO anything better
    QModelIndex anIndex = this->indexAt(QPoint(0,i));
    if (anIndex.isValid()) {
      qDebug() << "This index is valid";
    }
  }
}

void CallTreeView::removeCurrent() const
{
	QModelIndex index = selectionModel()->currentIndex();
	treeModel->removeRow(index.row(), index.parent());
}

CallTreeItem* CallTreeView::currentItem()
{
	QModelIndex index = selectionModel()->currentIndex();		

	CallTreeItem *item = treeModel->getItem(index);

	if (!item->call())
	{
		return 0;
	}
	return item;		
}

CallTreeItem* CallTreeView::getItem(const QModelIndex &index)
{
	return treeModel->getItem(index);
}

void CallTreeView::setCurrentRow(int row)
{
	CallTreeModel * treeModel = static_cast<CallTreeModel*>(model());

	QModelIndex currentIndex = selectionModel()->currentIndex();
	QModelIndex index = treeModel->index(row, 0, currentIndex);
	selectionModel()->setCurrentIndex(index,  QItemSelectionModel::Current);
}

int CallTreeView::count()
{
	return model()->rowCount();
}

QStringList CallTreeView::mimeTypes() const
{
  
}

Qt::DropActions CallTreeView::supportedDropActions () const
{
  return Qt::CopyAction | Qt::MoveAction;
}

void CallTreeView::itemClicked(const QModelIndex& anIndex) 
{
  if (currentModel != anIndex)
    emit itemChanged();
  currentModel = anIndex;
}

void CallTreeView::adaptColumns (const QModelIndex & topleft, const QModelIndex& bottomRight)
{
  int firstColumn= topleft.column();
  int lastColumn = bottomRight.column();
  do {
    //if (firstColumn) //TODO remove this and fix the resulting bug
      resizeColumnToContents(firstColumn);
    firstColumn++;
  } while (firstColumn < lastColumn);
}