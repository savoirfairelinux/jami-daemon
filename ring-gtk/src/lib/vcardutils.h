/****************************************************************************
 *   Copyright (C) 2013-2014 by Savoir-Faire Linux                           *
 *   Author : Alexandre Lision <alexandre.lision@savoirfairelinux.com> *
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

#ifndef VCARDUTILS_H
#define VCARDUTILS_H

#include "typedefs.h"
#include <QStringList>



class VCardUtils
{
public:

   struct Delimiter {
      constexpr static const char* VC_SEPARATOR_TOKEN    =  ";";
      constexpr static const char* VC_END_LINE_TOKEN     =  "\n";
      constexpr static const char* VC_BEGIN_TOKEN        =  "BEGIN:VCARD";
      constexpr static const char* VC_END_TOKEN          =  "END:VCARD";
   };


   // struc
   struct Property {
      constexpr static const char* VC_UID                 = "UID";
      constexpr static const char* VC_VERSION             = "VERSION";
      constexpr static const char* VC_ADDRESS             = "TOP";
      constexpr static const char* VC_AGENT               = "AGENT";
      constexpr static const char* VC_BIRTHDAY            = "BDAY";
      constexpr static const char* VC_CATEGORIES          = "CATEGORIES";
      constexpr static const char* VC_CLASS               = "CLASS";
      constexpr static const char* VC_DELIVERY_LABEL      = "LABEL";
      constexpr static const char* VC_EMAIL               = "EMAIL";
      constexpr static const char* VC_FORMATTED_NAME      = "FN";
      constexpr static const char* VC_GEOGRAPHIC_POSITION = "GEO";
      constexpr static const char* VC_KEY                 = "KEY";
      constexpr static const char* VC_LOGO                = "LOGO";
      constexpr static const char* VC_MAILER              = "MAILER";
      constexpr static const char* VC_NAME                = "N";
      constexpr static const char* VC_NICKNAME            = "NICKNAME";
      constexpr static const char* VC_NOTE                = "NOTE";
      constexpr static const char* VC_ORGANIZATION        = "ORG";
      constexpr static const char* VC_PHOTO               = "PHOTO";
      constexpr static const char* VC_PRODUCT_IDENTIFIER  = "PRODID";
      constexpr static const char* VC_REVISION            = "REV";
      constexpr static const char* VC_ROLE                = "ROLE";
      constexpr static const char* VC_SORT_STRING         = "SORT-STRING";
      constexpr static const char* VC_SOUND               = "SOUND";
      constexpr static const char* VC_TELEPHONE           = "TEL";
      constexpr static const char* VC_TIME_ZONE           = "TZ";
      constexpr static const char* VC_TITLE               = "TITLE";
      constexpr static const char* VC_URL                 = "URL";

      constexpr static const char* VC_X_RINGACCOUNT       = "X-RINGACCOUNTID";
   };

   VCardUtils();

   void startVCard(const QString& version);
   void addProperty(const char* prop, const QString& value);
   void addPhoneNumber(QString type, QString num);
   void addPhoto(const QByteArray img);
   const QByteArray endVCard();

private:

   //Attributes
   QStringList m_vCard;

};

#endif // VCARDUTILS_H
