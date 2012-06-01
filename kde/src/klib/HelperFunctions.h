/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/

#ifndef HELPER_FUNCTIONS
#define HELPER_FUNCTIONS

//Qt
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QHash>
#include <QtCore/QList>

//SFLPhone
#include "../lib/Contact.h"

//Typedef
typedef QHash<QString,QHash<QString,QVariant> > ContactHash;

///@class HelperFunctions little visitor not belonging to libqtsflphone
///Ramdom mix of dynamic property and transtypping
class LIB_EXPORT HelperFunctions {
public:
   static ContactHash toHash(QList<Contact*> contacts);
   static QString normStrippped(QString str);
};
#endif