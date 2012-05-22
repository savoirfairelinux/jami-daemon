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

#include "HelperFunctions.h"

//Qt
#include <QtCore/QString>
#include <QtCore/QVariant>

//KDE
#include <KLocale>

//SFLPhone
#include "../lib/Contact.h"

///Transform a contact list to a [QString][QString][QVariant] hash
ContactHash HelperFunctions::toHash(QList<Contact*> contacts) {
   QHash<QString,QHash<QString,QVariant> > hash;
   for (int i=0;i<contacts.size();i++) {
      Contact* cont = contacts[i];
      QHash<QString,QVariant> conth = cont->toHash();
      conth["phoneCount"] = cont->getPhoneNumbers().size();
      if (cont->getPhoneNumbers().size() == 1) {
         conth["phoneNumber"] = cont->getPhoneNumbers()[0]->getNumber();
         conth["phoneType"  ] = cont->getPhoneNumbers()[0]->getType();
      }
      else {
         conth["phoneNumber"] = QString::number(cont->getPhoneNumbers().size())+i18n(" numbers");
         conth["phoneType"  ] = "";
      }
      hash[contacts[i]->getUid()] = conth;
   }
   return hash;
}