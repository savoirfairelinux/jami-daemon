#ifndef METATYPES_H
#define METATYPES_H

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtDBus/QtDBus>
#include <qmap.h>
#include <qstring.h>

//class MapStringString:public QMap<QString, QString>{};
typedef QMap<QString, QString> MapStringString;
typedef QVector<QString> VectorString;

Q_DECLARE_METATYPE(MapStringString)
//Q_DECLARE_METATYPE(VectorString)


inline void registerCommTypes() {
	qDBusRegisterMetaType<MapStringString>();
	//qDBusRegisterMetaType<VectorString>();
}

#endif