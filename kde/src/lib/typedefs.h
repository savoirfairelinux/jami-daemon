/****************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                               *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#ifndef TYPEDEFS_H
#define TYPEDEFS_H

#include <QtCore/QMetaType>
#include <QtCore/QMap>
#include <QVector>
#include <QtCore/QString>

typedef QMap<QString, QString> MapStringString;
typedef QVector< QMap<QString, QString> > VectorMapStringString;
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

#if __GNUC__ < 4 || \
              (__GNUC__ == 5 && (__GNUC_MINOR__ < 5 || \
                                 (__GNUC_MINOR__ == 5)))
#define nullptr 0
#endif


#endif
