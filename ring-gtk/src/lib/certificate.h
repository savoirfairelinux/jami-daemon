/****************************************************************************
 *   Copyright (C) 2014 by Savoir-Faire Linux                               *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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
#ifndef CERTIFICATE_H
#define CERTIFICATE_H

#include "typedefs.h"

//Qt
#include <QUrl>

class LIB_EXPORT Certificate : public QObject {
   Q_OBJECT
public:

   //Structures
   enum class Type {
      AUTHORITY  ,
      USER       ,
      PRIVATE_KEY,
      NONE       ,
   };

   explicit Certificate(Certificate::Type type ,const QObject* parent = nullptr);

   //Getter
   QUrl path() const;
   Certificate::Type type() const;

   //Setter
   void setPath(const QUrl& path);

   //Validation
   bool exist                    () const;
   bool isExpired                () const;
   bool isSelfSigned             () const;
   bool hasPrivateKey            () const;
   bool hasProtectedPrivateKey   () const;
   bool hasRightPermissions      () const;
   bool hasRightFolderPermissions() const;
   bool isLocationSecure         () const;

private:
   QUrl m_Path;
   Certificate::Type m_Type;
   //TODO

Q_SIGNALS:
   void changed();
};
Q_DECLARE_METATYPE(Certificate*)

#endif
