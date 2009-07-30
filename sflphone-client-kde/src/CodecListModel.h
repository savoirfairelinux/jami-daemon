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
#ifndef CODECLISTMODEL_H
#define CODECLISTMODEL_H

#include <QAbstractItemModel>
#include "Codec.h"

/**
	@author Jérémy Quentin <jeremy.quentin@gmail.com>
*/
class CodecListModel : public QAbstractTableModel
{
Q_OBJECT
private:
	QList<Codec *> codecs;

public:
	CodecListModel(QObject *parent = 0);

	~CodecListModel();

	QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
	int rowCount(const QModelIndex & parent = QModelIndex()) const;
	int columnCount(const QModelIndex & parent = QModelIndex()) const;
	QVariant headerData(int section , Qt::Orientation orientation, int role) const;
	Qt::ItemFlags flags(const QModelIndex & index) const;
	virtual bool setData ( const QModelIndex & index, const QVariant &value, int role);
	
	bool codecUp( int index );
	bool codecDown( int index );
	QStringList getActiveCodecList() const ;
	void setActiveCodecList(const QStringList & activeCodecListToSet);

signals:
// 	void dataChanged(const QModelIndex &, const QModelIndex &);
};

#endif
