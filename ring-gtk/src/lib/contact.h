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

#ifndef CONTACT_H
#define CONTACT_H

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <time.h>

//Qt
class QPixmap;

//KDE
namespace KABC {
   class Addressee   ;
   class Picture     ;
   class PhoneNumber ;
}

//SFLPhone
class PhoneNumber;
class AbstractContactBackend;
class ContactPrivate;
class Account;

#include "typedefs.h"
#include "categorizedcompositenode.h"


///Contact: Abstract version of a contact
class LIB_EXPORT Contact : public QObject {
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
public:
   friend class ContactPrivate;

   class  PhoneNumbers : public QVector<PhoneNumber*>, public CategorizedCompositeNode {
   public:
      virtual QObject* getSelf() const __attribute__ ((const));
      explicit PhoneNumbers(Contact* parent);
      PhoneNumbers(Contact* parent, const QVector<PhoneNumber*>& list);
      Contact* contact() const;
      time_t lastUsedTimeStamp() const;
   private:
      Contact* m_pParent2       ;
   };

   //Properties
   Q_PROPERTY( PhoneNumbers          phoneNumbers   READ phoneNumbers   WRITE setPhoneNumbers                        )
   Q_PROPERTY( QString               nickName       READ nickName       WRITE setNickName                            )
   Q_PROPERTY( QString               firstName      READ firstName      WRITE setFirstName                           )
   Q_PROPERTY( QString               secondName     READ secondName     WRITE setFamilyName                          )
   Q_PROPERTY( QString               formattedName  READ formattedName  WRITE setFormattedName                       )
   Q_PROPERTY( QString               organization   READ organization   WRITE setOrganization                        )
   Q_PROPERTY( QByteArray            uid            READ uid            WRITE setUid                                 )
   Q_PROPERTY( QString               preferredEmail READ preferredEmail WRITE setPreferredEmail                      )
//    Q_PROPERTY( QPixmap*              photo          READ photo          WRITE setPhoto                               )
   Q_PROPERTY( QString               group          READ group          WRITE setGroup                               )
   Q_PROPERTY( QString               department     READ department     WRITE setDepartment                          )
   Q_PROPERTY( bool                  active         READ isActive       WRITE setActive         NOTIFY statusChanged )

   //Mutator
   Q_INVOKABLE bool save() const;
   Q_INVOKABLE bool edit()      ;
   Q_INVOKABLE bool remove()    ;
   Q_INVOKABLE bool addPhoneNumber(PhoneNumber* n);

protected:
   //The D-Pointer can be shared if a PlaceHolderContact is merged with a real one
   ContactPrivate* d;
   void replaceDPointer(Contact* other);

public:
   //Constructors & Destructors
   explicit Contact(AbstractContactBackend* parent = nullptr);
   //Contact(AbstractContactBackend *parent = nullptr, const QByteArray& vcard = nullptr);
   virtual ~Contact();

   //Getters
   const PhoneNumbers& phoneNumbers() const;
   const QString& nickName         () const;
   const QString& firstName        () const;
   const QString& secondName       () const;
   const QString& formattedName    () const;
   const QString& organization     () const;
   const QByteArray& uid           () const;
   const QString& preferredEmail   () const;
   const QPixmap* photo            () const;
   const QString& group            () const;
   const QString& department       () const;
   bool  isActive                  () const;

   //Cache
   QString filterString            () const;

   //Number related getters (proxies)
   bool isPresent                  () const;
   bool isTracked                  () const;
   bool supportPresence            () const;

   //Setters
   void setPhoneNumbers   ( PhoneNumbers             );
   void setFormattedName  ( const QString&    name   );
   void setNickName       ( const QString&    name   );
   void setFirstName      ( const QString&    name   );
   void setFamilyName     ( const QString&    name   );
   void setOrganization   ( const QString&    name   );
   void setPreferredEmail ( const QString&    name   );
   void setGroup          ( const QString&    name   );
   void setDepartment     ( const QString&    name   );
   void setUid            ( const QByteArray& id     );
   void setPhoto          ( QPixmap*          photo  );
   void setActive         ( bool              active );

   //Operator
   bool operator==(Contact* other);
   bool operator==(const Contact* other) const;
   bool operator==(Contact& other);
   bool operator==(const Contact& other) const;

   //VCard Serialization
   const QByteArray toVCard(QList<Account*> accounts) const;

private Q_SLOTS:
   void slotPresenceChanged();

Q_SIGNALS:
   void presenceChanged( PhoneNumber* );
   void statusChanged  ( bool         );
   void changed        (              );
   void phoneNumberCountChanged(int,int);
   void phoneNumberCountAboutToChange(int,int);
   void rebased        ( Contact*     );

protected:
   //Presence secret methods
   void updatePresenceInformations(const QString& uri, bool status, const QString& message);
};

class LIB_EXPORT ContactPlaceHolder : public Contact {
   Q_OBJECT
public:
   ContactPlaceHolder(const QByteArray& uid);
   bool merge(Contact* contact);
};


Q_DECLARE_METATYPE(Contact*)

#endif
