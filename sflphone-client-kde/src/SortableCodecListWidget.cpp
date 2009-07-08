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

#include "CodecListModel.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDebug>

SortableCodecListWidget::SortableCodecListWidget(QWidget *parent)
 : QWidget(parent)
{
	codecTable = new QTableView(this);
	codecTable->setObjectName("codecTable");
	CodecListModel * model = new CodecListModel();
	codecTable->setModel(model);
	codecTable->resizeColumnsToContents();
	codecTable->resizeRowsToContents();
	codecTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	
	codecUpButton = new KPushButton(this);
	codecUpButton->setObjectName("codecUpButton");
	codecUpButton->setIcon(KIcon("go-up"));
	
	codecDownButton = new KPushButton(this);
	codecDownButton->setObjectName("codecDownButton");
	codecDownButton->setIcon(KIcon("go-down"));
	
	QHBoxLayout * mainLayout = new QHBoxLayout(this);
	QVBoxLayout * buttonsLayout = new QVBoxLayout(this);
	
	buttonsLayout->addWidget(codecUpButton);
	buttonsLayout->addWidget(codecDownButton);
	
	mainLayout->addWidget(codecTable);
	mainLayout->addLayout(buttonsLayout);
	
	this->setLayout(mainLayout);
	
	QMetaObject::connectSlotsByName(this);
}

void SortableCodecListWidget::on_codecTable_currentCellChanged(int currentRow)
{
	qDebug() << "on_codecTable_currentCellChanged";
// 	int nbCol = codecTable->model()->columnCount();
// 	for(int i = 0 ; i < nbCol ; i++)
// 	{
// 		codecTable->setRangeSelected(QTableWidgetSelectionRange(currentRow, 0, currentRow, nbCol - 1), true);
// 	}
	updateCommands();
}

void SortableCodecListWidget::on_codecUpButton_clicked()
{
	qDebug() << "on_toolButton_codecUpButton_clicked";
	CodecListModel * model = (CodecListModel *) codecTable->model();
	int currentRow = selectedRow();
	model->codecUp(currentRow);
	setSelectedRow(currentRow - 1);
	
// 	int currentCol = codecTable->currentColumn();
// 	int currentRow = codecTable->currentRow();
// 	int nbCol = codecTable->columnCount();
// 	for(int i = 0 ; i < nbCol ; i++)
// 	{
// 		QTableWidgetItem * item1 = tableWidget_codecs->takeItem(currentRow, i);
// 		QTableWidgetItem * item2 = tableWidget_codecs->takeItem(currentRow - 1, i);
// 		tableWidget_codecs->setItem(currentRow - 1, i , item1);
// 		tableWidget_codecs->setItem(currentRow, i , item2);
// 	}
// 	QTableWidgetItem * item1 = tableWidget_codecs->takeVerticalHeaderItem(currentRow);
// 	QTableWidgetItem * item2 = tableWidget_codecs->takeVerticalHeaderItem(currentRow - 1);
// 	tableWidget_codecs->setVerticalHeaderItem(currentRow - 1, item1);
// 	tableWidget_codecs->setVerticalHeaderItem(currentRow, item2);
// 	tableWidget_codecs->setCurrentCell(currentRow - 1, currentCol);
}

void SortableCodecListWidget::on_codecDownButton_clicked()
{
	qDebug() << "on_codecDownButton_clicked";
	CodecListModel * model = (CodecListModel *) codecTable->model();
	int currentRow = selectedRow();
	model->codecDown(currentRow);
	setSelectedRow(currentRow + 1);
	
// 	int currentCol = tableWidget_codecs->currentColumn();
// 	int currentRow = tableWidget_codecs->currentRow();
// 	int nbCol = tableWidget_codecs->columnCount();
// 	for(int i = 0 ; i < nbCol ; i++)
// 	{
// 		QTableWidgetItem * item1 = tableWidget_codecs->takeItem(currentRow, i);
// 		QTableWidgetItem * item2 = tableWidget_codecs->takeItem(currentRow + 1, i);
// 		tableWidget_codecs->setItem(currentRow + 1, i , item1);
// 		tableWidget_codecs->setItem(currentRow, i , item2);
// 	}
// 	QTableWidgetItem * item1 = tableWidget_codecs->takeVerticalHeaderItem(currentRow);
// 	QTableWidgetItem * item2 = tableWidget_codecs->takeVerticalHeaderItem(currentRow + 1);
// 	tableWidget_codecs->setVerticalHeaderItem(currentRow + 1, item1);
// 	tableWidget_codecs->setVerticalHeaderItem(currentRow, item2);
// 	tableWidget_codecs->setCurrentCell(currentRow + 1, currentCol);
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
	QItemSelection newSelection = QItemSelection(model->index(row, 0, QModelIndex()), model->index(row, model->columnCount(), QModelIndex()));
	selection->clear();
// 	selection->select(newSelection , QItemSelectionModel::Select);
	selection->select(model->index(row, model->columnCount()-1, QModelIndex()) , QItemSelectionModel::Select);
// 	listView->selectionModel()->select(belowIndex, QItemSelectionModel::Select);
} 