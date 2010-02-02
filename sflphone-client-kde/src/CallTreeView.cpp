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

#include "CallTreeView.h"
#include "CallTreeModel.h"
#include "CallTreeItem.h"
#include "Call.h"

CallTreeView::CallTreeView(QWidget * parent)
	: QTreeView(parent)
{
	model = new TreeModel(this);

	setModel(model)
}

CallTreeView::~CallTreeView()
{
	delete modele;
}

CallTreeItem* CallTreeView::insert(Call *call)
{
	QModelIndex index = selectionModel()->currentIndex();
	QAbstractItemModel *model = model();

	if (!model->insertRow(index.row()+1, index.parent()))
	{
		return;
	}

	for (int column = 0; column < model->columnCount(index.parent()); ++column) 
	{
		QModelIndex child = model->index(index.row()+1, column, index.parent());
		model->setData(child, call, Qt::EditRole);
	}
}

CallTreeItem* CallTreeView::insert(CallTreeItem *parent, Call *call)
{


QModelIndex index = view->selectionModel()->currentIndex();
{
     QAbstractItemModel *model = model();

     if (model->columnCount(index) == 0) 
     {
         if (!model->insertColumn(0, index))
	 {
		 return;
	 }

     }

     if (!model->insertRow(0, index))
     {
         return;
     }

     for (int column = 0; column < model->columnCount(index); ++column) 
     {
         QModelIndex child = model->index(0, column, index);
         model->setData(child, QVariant("[No data]"), Qt::EditRole);
         if (!model->headerData(column, Qt::Horizontal).isValid())
             model->setHeaderData(column, Qt::Horizontal, QVariant("[No header]"),
                                  Qt::EditRole);
     }

     view->selectionModel()->setCurrentIndex(model->index(0, 0, index),
                                             QItemSelectionModel::ClearAndSelect);
}

void CallTreeView::remove(CallTreeItem *item)
{
	// to implement
}


