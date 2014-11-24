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

#include "vcardutils.h"
#include <QBuffer>

//BEGIN:VCARD
//VERSION:3.0
//N:Gump;Forrest
//FN:Forrest Gump
//ORG:Bubba Gump Shrimp Co.
//TITLE:Shrimp Man
//PHOTO;VALUE=URL;TYPE=GIF:http://www.example.com/dir_photos/my_photo.gif
//TEL;TYPE=WORK,VOICE:(111) 555-1212
//TEL;TYPE=HOME,VOICE:(404) 555-1212
//ADR;TYPE=WORK:;;100 Waters Edge;Baytown;LA;30314;United States of America
//LABEL;TYPE=WORK:100 Waters Edge\nBaytown, LA 30314\nUnited States of America
//ADR;TYPE=HOME:;;42 Plantation St.;Baytown;LA;30314;United States of America
//LABEL;TYPE=HOME:42 Plantation St.\nBaytown, LA 30314\nUnited States of America
//EMAIL;TYPE=PREF,INTERNET:forrestgump@example.com
//REV:20080424T195243Z
//END:VCARD

//4.6 Image Restrictions
//The following rules apply to images:
//1. The image SHOULD use less than eight kilobytes (8k) of data; this restriction
//   is to be enforced by the publishing client.
//2. The image height and width SHOULD be between thirty-two (32) and ninety-six (96) pixels;
//   the recommended size is sixty-four (64) pixels high and sixty-four (64) pixels wide.
//3. The image SHOULD be square.
//4. The image content type SHOULD be image/gif, image/jpeg, or image/png;
//   support for the "image/png" content type is REQUIRED,
//   support for the "image/gif" and "image/jpeg" content types is RECOMMENDED,
//   and support for any other content type is OPTIONAL.

VCardUtils::VCardUtils()
{

}

void VCardUtils::startVCard(const QString& version)
{
   m_vCard << Delimiter::BEGIN_TOKEN;
   addProperty(Property::VERSION, version);
}

void VCardUtils::addProperty(const char* prop, const QString& value)
{
   if(value.isEmpty() || value == ";")
      return;
   m_vCard << QString(QString::fromUtf8(prop) + ":" + value);
}

void VCardUtils::addPhoneNumber(QString type, QString num)
{
   // This will need some formatting
   addProperty(Property::TELEPHONE, type + num);
//   char* prop = VCProperty::VC_TELEPHONE;
//   strcat(prop, VCDelimiter::VC_SEPARATOR_TOKEN);
//   strcat(prop, "TYPE=" + type);
//   strcat(prop, ",VOICE");
}

void VCardUtils::addPhoto(const QByteArray img)
{
   m_vCard << QString(QString::fromUtf8(Property::PHOTO) +
                      QString::fromUtf8(Delimiter::SEPARATOR_TOKEN) +
                      "ENCODING=BASE64" +
                      QString::fromUtf8(Delimiter::SEPARATOR_TOKEN) +
                      "TYPE=PNG:" +
                      img.toBase64());
}

const QByteArray VCardUtils::endVCard()
{
   m_vCard << Delimiter::END_TOKEN;
   const QString result = m_vCard.join(QString::fromUtf8(Delimiter::END_LINE_TOKEN));
   return result.toUtf8();
}
