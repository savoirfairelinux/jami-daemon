/***************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                              *
 *   Author : Jérémy Quentin                                               *
 *   jeremy.quentin@savoirfairelinux.com                                   *
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
#include "SortableCodecListWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDebug>

SortableCodecListWidget::SortableCodecListWidget(QWidget *parent)
 : QWidget(parent)
{
	codecTable = new QTableView(this);
	codecTable->setObjectName("codecTable");
	codecTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	
	codecUpButton = new KPushButton(this);
	codecUpButton->setObjectName("codecUpButton");
	codecUpButton->setIcon(KIcon("go-up"));
	
	codecDownButton = new KPushButton(this);
	codecDownButton->setObjectName("codecDownButton");
	codecDownButton->setIcon(KIcon("go-down"));
	
	QHBoxLayout * mainLayout = new QHBoxLayout(this);
	QVBoxLayout * buttonsLayout = new QVBoxLayout();
	
	buttonsLayout->addWidget(codecUpButton);
	buttonsLayout->addWidget(codecDownButton);
	
	mainLayout->addWidget(codecTable);
	mainLayout->addLayout(buttonsLayout);
	
	QMetaObject::connectSlotsByName(this);
}

void SortableCodecListWidget::setModel(CodecListModel * model)
{
	codecTable->setModel(model);
	codecTable->resizeColumnsToContents();
	codecTable->resizeRowsToContents();	
	connect(codecTable->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
	        this,                         SLOT(updateCommands()));
	connect(codecTable->model(),          SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)),
	        this,                         SIGNAL(dataChanged()));
}

CodecListModel * SortableCodecListWidget::model()
{
	return (CodecListModel *) codecTable->model();
}

void SortableCodecListWidget::on_codecUpButton_clicked()
{
	qDebug() << "on_toolButton_codecUpButton_clicked";
	CodecListModel * model = (CodecListModel *) codecTable->model();
	int currentRow = selectedRow();
	model->codecUp(currentRow);
	setSelectedRow(currentRow - 1);
}

void SortableCodecListWidget::on_codecDownButton_clicked()
{
	qDebug() << "on_codecDownButton_clicked";
	CodecListModel * model = (CodecListModel *) codecTable->model();
	int currentRow = selectedRow();
	model->codecDown(currentRow);
	setSelectedRow(currentRow + 1);
}

void SortableCodecListWidget::updateCommands()
{
	qDebug() << "SortableCodecListWidget::updateCommands";
	bool buttonsEnabled[2] = {true,true};
	if(selectedRow() == -1)
	{
		buttonsEnabled[0] = false;
		buttonsEnabled[1] = false;
	}
	else
	{
		if(selectedRow() == 0)
		{
			buttonsEnabled[0] = false;
		}
		if(selectedRow() == codecTable->model()->rowCount() - 1)
		{
			buttonsEnabled[1] = false;
		}
	}
	codecUpButton->setEnabled(buttonsEnabled[0]);
	codecDownButton->setEnabled(buttonsEnabled[1]);
}

QModelIndex SortableCodecListWidget::selectedIndex()
{
	QItemSelectionModel *selection = codecTable->selectionModel();
	const QModelIndexList selectedIndexes = selection->selectedIndexes();
	if ( !selectedIndexes.isEmpty() && selectedIndexes[0].isValid() )
		return selectedIndexes[0];
	else
		return QModelIndex();
 }

int SortableCodecListWidget::selectedRow()
{
	QModelIndex index = selectedIndex();
	if(index.isValid())
		return index.row();
	else
		return -1;
}
 
void SortableCodecListWidget::setSelectedRow(int row)
{
	QItemSelectionModel * selection = codecTable->selectionModel();
	QAbstractItemModel * model = codecTable->model();
	QItemSelection newSelection = QItemSelection(model->index(row, 0, QModelIndex()), model->index(row +1 , model->columnCount(), QModelIndex()));
	selection->clear();
	for(int i = 0 ; i < model->columnCount() ; i++)
	{
		selection->select(model->index(row, i, QModelIndex()) , QItemSelectionModel::Select);
	}
} 