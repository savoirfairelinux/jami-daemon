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

CallTreeView::CallTreeView(QWidget * parent)
	: QTreeView(parent)
{	 
	treeModel = new CallTreeModel(this);
	setModel(treeModel);
	setHeaderHidden(true);
	setRootIsDecorated(false);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setDragEnabled(true);
	setAcceptDrops(true);
	setUniformRowHeights(true);
	setDropIndicatorShown(true);
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

	item = treeModel->getItem(child);
	item->setCall(call);
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

bool CallTreeView::dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{/*
	 = data->data("items/index");
 
	foreach(QModelIndex index, callList) // iterate over list
	{
		CallTreeItem *item = static_cast<CallTreeItem*>(index.internalPointer());

		if (parent == NULL)
		{
			// make new QTreeWidgetItem and set its text
			// if parent is null - add top level item (this parent)
			insert(item->call());
		}
		else 
		{    
			// else add QTreeWidgetItem with parent and expand parent
			insert(parent, item->call());
			parent->setExpanded( true );
		}
	}	*/
	return true;    
}

QStringList CallTreeView::mimeTypes() const
{

}


Qt::DropActions CallTreeView::supportedDropActions () const
{

}
