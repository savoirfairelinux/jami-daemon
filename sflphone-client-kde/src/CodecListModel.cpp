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
#include "CodecListModel.h"

#include <QtCore/QSize>
#include <QtCore/QDebug>
#include <KLocale>
#include "configurationmanager_interface_singleton.h"

CodecListModel::CodecListModel(QObject *parent)
 : QAbstractTableModel(parent)
{
	this->codecs = QList<Codec *>();
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	QStringList activeCodecList = configurationManager.getActiveCodecList();
	setActiveCodecList(activeCodecList);
}


CodecListModel::~CodecListModel()
{
}


QVariant CodecListModel::data ( const QModelIndex & index, int role) const
{
	if (!index.isValid())
		return QVariant();

	const Codec * codec = codecs[index.row()];
	if(index.column() == 0 && role == Qt::DisplayRole)
	{
		return QVariant(codec->getName());
	}
	else if(index.column() == 0 && role == Qt::CheckStateRole)
	{
		return QVariant(codec->isEnabled() ? Qt::Checked : Qt::Unchecked);
	}
	else if(index.column() == 1 && role == Qt::DisplayRole)
	{
		return QVariant(codec->getFrequency());
	}
	else if(index.column() == 2 && role == Qt::DisplayRole)
	{
		return QVariant(codec->getBitrate());
	}
	else if(index.column() == 3 && role == Qt::DisplayRole)
	{
		return QVariant(codec->getBandwidth());
	}
	
	return QVariant();
}


int CodecListModel::rowCount(const QModelIndex & /*parent*/) const
{
	return codecs.count();
}

int CodecListModel::columnCount(const QModelIndex & /*parent*/) const
{
	return 4;
}


QVariant CodecListModel::headerData(int section , Qt::Orientation orientation, int role) const
{
	if (section == 0 && orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		return QVariant(i18n("Codec"));
	}
	else if (section == 1 && orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		return QVariant(i18n("Frequency"));
	}
	else if (section == 2 && orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		return QVariant(i18n("Bitrate"));
	}
	else if (section == 3 && orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		return QVariant(i18n("Bandwidth"));
	}
	return QVariant();
}

Qt::ItemFlags CodecListModel::flags(const QModelIndex & index) const
{
	if (index.column() == 0)
	{
		return QAbstractItemModel::flags(index) | Qt::ItemIsUserCheckable;
	}
	return QAbstractItemModel::flags(index);
}

bool CodecListModel::setData ( const QModelIndex & index, const QVariant &value, int role)
{
	qDebug() << "setData";
	if (index.isValid() && index.column() == 0 && role == Qt::CheckStateRole) {
		codecs[index.row()]->setEnabled(value.toBool());
		emit dataChanged(index, index);
		return true;
	}
	return false;
}

bool CodecListModel::codecUp( int index )
{
	if(index > 0 && index <= rowCount())
	{
		codecs.swap(index - 1, index);
		emit dataChanged(this->index(index - 1, 0, QModelIndex()), this->index(index, columnCount(), QModelIndex()));
		return true;
	}
	return false;
}

bool CodecListModel::codecDown( int index )
{
	if(index >= 0 && index < rowCount())
	{
		codecs.swap(index + 1, index);
		emit dataChanged(this->index(index, 0, QModelIndex()), this->index(index + 1, columnCount(), QModelIndex()));
		return true;
	}
	return false;
}

QStringList CodecListModel::getActiveCodecList() const
{
	QStringList codecList;
	for(int i = 0 ; i < rowCount() ; i++)
	{
		if(codecs[i]->isEnabled())
			codecList.append(codecs[i]->getPayload());
	}
	return codecList;
}

void CodecListModel::setActiveCodecList(const QStringList & activeCodecListToSet)
{
	this->codecs = QList<Codec *>();
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	QStringList codecList = configurationManager.getCodecList();
	QStringList activeCodecList = activeCodecListToSet;
	#if QT_VERSION >= 0x040500
		activeCodecList.removeDuplicates();
	#else
   	for (int i = 0 ; i < activeCodecList.size() ; i++)
		{
			if(activeCodecList.lastIndexOf(activeCodecList[i]) != i || ! codecList.contains(activeCodecList[i]))
			{
				activeCodecList.removeAt(i);
				i--;
			}
		}
	#endif

	QStringList codecListToDisplay = activeCodecList;
	for (int i=0 ; i<codecList.size() ; i++)
	{
		if(! activeCodecList.contains(codecList[i]))
		{
			codecListToDisplay << codecList[i];
		}
	}
	for(int i=0 ; i<codecListToDisplay.size() ; i++)
	{
		bool ok;
		QString payloadStr = QString(codecListToDisplay[i]);
		int payload = payloadStr.toInt(&ok);
		if(!ok)	
			qDebug() << "The codec's payload sent by the configurationManager is not a number : " << codecListToDisplay[i];
		else
		{
			codecs << new Codec(payload, activeCodecList.contains(codecListToDisplay[i]));
		}
	}
	
	emit dataChanged(this->index(0, 0, QModelIndex()), this->index(rowCount(), columnCount(), QModelIndex()));
}
