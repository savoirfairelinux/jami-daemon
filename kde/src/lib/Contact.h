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

#ifndef CONTACT_H
#define CONTACT_H

#include <QtCore/QObject>
#include <QtCore/QVariant>

//Qt
class QListWidgetItem;
class QWidget;
class QPixmap;

//KDE
namespace KABC {
   class Addressee   ;
   class Picture     ;
   class PhoneNumber ;
}

#include "typedefs.h"

///@class Contact Abstract version of a contact
class LIB_EXPORT Contact : public QObject{
   Q_OBJECT
public:
   class PhoneNumber {
   public:
      PhoneNumber(QString number, QString type)
      : m_Number(number),m_Type(type){}
      QString& getNumber() {
         return m_Number ;
      }
      QString& getType() {
         return m_Type   ;
      }
      
   private:
      QString m_Number   ;
      QString m_Type     ;
   };
   
   typedef QList<Contact::PhoneNumber*> PhoneNumbers;
   
private:
   QString      m_FirstName      ;
   QString      m_SecondName     ;
   QString      m_NickName       ;
   QPixmap*     m_pPhoto         ;
   QString      m_Type           ;
   QString      m_FormattedName  ;
   QString      m_PreferredEmail ;
   QString      m_Organization   ;
   QString      m_Uid            ;
   QString      m_Group          ;
   QString      m_Department     ;
   bool         m_DisplayPhoto   ;
   PhoneNumbers m_Numbers        ;
   
public:
   //Constructors & Destructors
   explicit Contact();
   virtual ~Contact();
   virtual void initItem();
   
   //Getters
   virtual PhoneNumbers   getPhoneNumbers()    const;
   virtual const QString& getNickName()        const;
   virtual const QString& getFirstName()       const;
   virtual const QString& getSecondName()      const;
   virtual const QString& getFormattedName()   const;
   virtual const QString& getOrganization()    const;
   virtual const QString& getUid()             const;
   virtual const QString& getPreferredEmail()  const;
   virtual const QPixmap* getPhoto()           const;
   virtual const QString& getType()            const;
   virtual const QString& getGroup()           const;
   virtual const QString& getDepartment()      const;

   //Setters
   virtual void setPhoneNumbers   ( PhoneNumbers        );
   virtual void setFormattedName  ( const QString& name );
   virtual void setNickName       ( const QString& name );
   virtual void setFirstName      ( const QString& name );
   virtual void setFamilyName     ( const QString& name );
   virtual void setOrganization   ( const QString& name );
   virtual void setPreferredEmail ( const QString& name );
   virtual void setGroup          ( const QString& name );
   virtual void setDepartment     ( const QString& name );
   virtual void setUid            ( const QString& id   );
   virtual void setPhoto          ( QPixmap* photo      );

   //Mutator
   QHash<QString,QVariant> toHash();
   
protected:
   virtual void initItemWidget();

};
typedef Contact::PhoneNumbers PhoneNumbers;

#endif
