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

//Parent
#include "ContactBackend.h"

//SFLPhone library
#include "Contact.h"

//Qt
#include <QtCore/QHash>

///Constructor
ContactBackend::ContactBackend(QObject* parent) : QObject(parent)
{
   
}

///Destructor
ContactBackend::~ContactBackend()
{
   foreach (Contact* c,m_ContactByUid) {
      delete c;
   }
}

///Update slot
ContactList ContactBackend::update()
{
   return update_slot();
}

/*****************************************************************************
 *                                                                           *
 *                                  Helpers                                  *
 *                                                                           *
 ****************************************************************************/

///Return the extension/user of an URI (<sip:12345@exemple.com>)
QString ContactBackend::getUserFromPhone(QString phoneNumber)
{
   if (phoneNumber.indexOf("@") != -1) {
      QString user = phoneNumber.split("@")[0];
      if (user.indexOf(":") != -1) {
         return user.split(":")[1];
      }
      else {
         return user;
      }
   }
   return phoneNumber;
} //getUserFromPhone

///Return the domaine of an URI (<sip:12345@exemple.com>)
QString ContactBackend::getHostNameFromPhone(QString phoneNumber)
{
   if (phoneNumber.indexOf("@") != -1) {
      return phoneNumber.split("@")[1].left(phoneNumber.split("@")[1].size()-1);
   }
   return "";
}