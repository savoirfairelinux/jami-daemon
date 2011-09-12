#ifndef TYPEDEFS_H
#define TYPEDEFS_H

#include <QtCore/QMetaType>
#include <QtCore/QMap>
#include <QtCore/QString>

typedef QMap<QString, QString> MapStringString;
typedef QMap<QString, int> MapStringInt;

//Mixe from LIB and GCC website
#if __GNUC__ >= 4
   #define LIB_NO_EXPORT __attribute__ ((visibility("hidden")))
   #define LIB_EXPORT __attribute__ ((visibility("default")))
   #define LIB_IMPORT __attribute__ ((visibility("default")))
#elif defined(_WIN32) || defined(_WIN64)
   #define LIB_NO_EXPORT
   #define LIB_EXPORT __declspec(dllexport)
   #define LIB_IMPORT __declspec(dllimport)
#else
   #define LIB_NO_EXPORT
   #define LIB_EXPORT
   #define LIB_IMPORT
#endif


#endif