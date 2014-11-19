/******************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                 *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>   *
 *            Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>            *
 *                                                                            *
 *   This library is free software; you can redistribute it and/or            *
 *   modify it under the terms of the GNU Lesser General Public               *
 *   License as published by the Free Software Foundation; either             *
 *   version 2.1 of the License, or (at your option) any later version.       *
 *                                                                            *
 *   This library is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU        *
 *   Lesser General Public License for more details.                          *
 *                                                                            *
 *   You should have received a copy of the Lesser GNU General Public License *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
 *****************************************************************************/
#ifndef METATYPES_H
#define METATYPES_H

#include <QtCore/QMetaType>
#include <QtCore/QMap>
#include <QVector>
#include <QtCore/QString>
#include <QtDBus/QtDBus>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
typedef QMap<QString, QString> MapStringString;
typedef QMap<QString, int> MapStringInt;
typedef QVector<int> VectorInt;
typedef QVector< QMap<QString, QString> > VectorMapStringString;
typedef QVector< QString > VectorString;
typedef QMap< QString, QMap< QString, QVector<QString> > > MapStringMapStringVectorString;
typedef QMap< QString, QVector<QString> > MapStringVectorString;

Q_DECLARE_METATYPE(MapStringString)
Q_DECLARE_METATYPE(MapStringInt)
Q_DECLARE_METATYPE(VectorMapStringString)
Q_DECLARE_METATYPE(MapStringMapStringVectorString)
Q_DECLARE_METATYPE(VectorInt)
Q_DECLARE_METATYPE(VectorString)
Q_DECLARE_METATYPE(MapStringVectorString)

static bool dbus_metaTypeInit = false;
inline void registerCommTypes() {
	qDBusRegisterMetaType<MapStringString>();
	qDBusRegisterMetaType<MapStringInt>();
	qDBusRegisterMetaType<VectorMapStringString>();
        qDBusRegisterMetaType<MapStringMapStringVectorString>();
        qDBusRegisterMetaType<VectorInt>();
	qDBusRegisterMetaType<VectorString>();
        qDBusRegisterMetaType<MapStringVectorString>();
   dbus_metaTypeInit = true;
}
#pragma GCC diagnostic pop

#endif
