/****************************************************************************
 *   Copyright (C) 2009-2014 by Savoir-Faire Linux                          *
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
#include <QtCore/QDebug>

typedef QMap<QString, QString> MapStringString;
typedef QVector< QMap<QString, QString> > VectorMapStringString;
typedef QMap< QString, QMap< QString, QVector<QString> > > MapStringMapStringVectorString;
typedef QMap<QString, int> MapStringInt;

template<class T, class E>
struct TypedStateMachine
{
    // no ctor/dtor and one public member variable for easy initialization
    T _data[size_t(E::__COUNT)];

    T& operator[](E v) {
      if (size_t(v) >= size_t(E::__COUNT)) {
         qDebug() << "State Machine Out of Bound" << size_t(v);
         throw v;
      }
      return _data[size_t(v)];
    }

    const T& operator[](E v) const {
      if (size_t(v) >= size_t(E::__COUNT)) {
         qDebug() << "State Machine Out of Bound" << size_t(v);
         throw v;
      }
      return _data[size_t(v)];
    }

    T *begin() {
      return _data;
    }

    T *end() {
      return _data + size_t(E::__COUNT);
    }
};

/**
 * This function add a safe way to get an enum class size
 * @note it cannot be "const" due to some compiler issues
 * @note it cannot be unsigned to avoid some compiler warnings
 */
template<typename A> constexpr int enum_class_size() {
   return static_cast<int>(A::__COUNT);
}

#define LIB_EXPORT Q_DECL_EXPORT
#define LIB_IMPORT Q_DECL_IMPORT

#if __GNUC__ < 4 || \
              (__GNUC__ == 5 && (__GNUC_MINOR__ < 5 || \
                                 (__GNUC_MINOR__ == 5)))
#define nullptr 0
#endif

//Doesn't work
#if ((__GNUC_MINOR__ > 8) || (__GNUC_MINOR__ == 8))
   #define STRINGIFY(x) #x
   #define IGNORE_NULL(content)\
   _Pragma(STRINGIFY(GCC diagnostic ignored "-Wzero-as-null-pointer-constant")) \
      content
#else
   #define IGNORE_NULL(content) content
#endif //ENABLE_IGNORE_NULL
#endif
