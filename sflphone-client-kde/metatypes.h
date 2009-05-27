#ifndef METATYPES_H
#define METATYPES_H

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtDBus/QtDBus>

typedef QMap<QString, QString> MapStringString;
typedef QMap<QString, int> MapStringInt;

Q_DECLARE_METATYPE(MapStringString)
Q_DECLARE_METATYPE(MapStringInt)


inline void registerCommTypes() {
	qDBusRegisterMetaType<MapStringString>();
	qDBusRegisterMetaType<MapStringInt>();
}

#endif