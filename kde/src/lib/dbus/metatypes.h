#ifndef METATYPES_H
#define METATYPES_H

#include <QtCore/QMetaType>
#include <QtCore/QMap>
#include <QVector>
#include <QtCore/QString>
#include <QtDBus/QtDBus>

typedef QMap<QString, QString> MapStringString;
typedef QMap<QString, int> MapStringInt;
typedef QVector<int> VectorInt;
typedef QVector< QMap<QString, QString> > VectorMapStringString;

Q_DECLARE_METATYPE(MapStringString)
Q_DECLARE_METATYPE(MapStringInt)
Q_DECLARE_METATYPE(VectorMapStringString)
Q_DECLARE_METATYPE(VectorInt);

static bool dbus_metaTypeInit = false;
inline void registerCommTypes() {
	qDBusRegisterMetaType<MapStringString>();
	qDBusRegisterMetaType<MapStringInt>();
	qDBusRegisterMetaType<VectorMapStringString>();
	qDBusRegisterMetaType<VectorInt>();
   dbus_metaTypeInit = true;
}

#endif
