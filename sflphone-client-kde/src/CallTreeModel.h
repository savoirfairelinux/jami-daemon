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

#ifndef CALLTREE_MODEL_H
#define CALLTREE_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>
#include <QtCore/QVariant>

class CallTreeItem;
class Call;

class CallTreeModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	CallTreeModel(QObject *parent = 0);
	~CallTreeModel();

	QVariant data(const QModelIndex &index, int role) const;
	Call* call(const QModelIndex &index, int role) const;

	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
	QModelIndex parent(const QModelIndex &index) const;

	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;

	Qt::ItemFlags flags(const QModelIndex &index) const;
	bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
	bool setCall(const QModelIndex &index, Call *value, int role = Qt::EditRole);
	bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role = Qt::EditRole);

	bool insertColumns(int position, int columns, const QModelIndex &parent = QModelIndex());
	bool removeColumns(int position, int columns, const QModelIndex &parent = QModelIndex());
	bool insertRows(int position, int rows, const QModelIndex &parent = QModelIndex());
	bool removeRows(int position, int rows, const QModelIndex &parent = QModelIndex());
        
        Qt::DropActions supportedDropActions();
        virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
        QStringList mimeTypes() const;
        QMimeData* mimeData(const QModelIndexList &indexes) const;

	CallTreeItem *getItem(const QModelIndex &index) const;

private:
	void setupModelData(const QStringList &lines, CallTreeItem *parent);
	CallTreeItem *rootItem;
        
signals:
        void joinCall(QString callId1, QString callId2);
        void over(Call*);
};

#endif // CALLTREE_MODEL_H
