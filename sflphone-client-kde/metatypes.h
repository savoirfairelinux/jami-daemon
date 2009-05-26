#ifndef METATYPES_H
#define METATYPES_H

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtDBus/QtDBus>

//class MapStringString:public QMap<QString, QString>{};
typedef QMap<QString, QString> MapStringString;
typedef QMap<QString, int> MapStringInt;
//typedef QVector<QString> VectorString;

Q_DECLARE_METATYPE(MapStringString)
Q_DECLARE_METATYPE(MapStringInt)
//Q_DECLARE_METATYPE(VectorString)


inline void registerCommTypes() {
	qDBusRegisterMetaType<MapStringString>();
	qDBusRegisterMetaType<MapStringInt>();
	//qDBusRegisterMetaType<VectorString>();
}

#endif