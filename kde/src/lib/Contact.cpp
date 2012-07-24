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
#include "Contact.h"

//Qt
#include <QtGui/QPixmap>

//SFLPhone library
#include "sflphone_const.h"

///Constructor
Contact::Contact():m_pPhoto(0)
{
   initItem();
}

///Destructor
Contact::~Contact()
{
   delete m_pPhoto;
   foreach (Contact::PhoneNumber* ph, m_Numbers) {
      delete ph;
   }
}

///May be used in extended classes
void Contact::initItem()
{
   initItemWidget();
}

///May be used in extended classes
void Contact::initItemWidget()
{
   
}

///Get the phone number list
PhoneNumbers Contact::getPhoneNumbers() const
{
   return m_Numbers;
}

///Get the nickname
const QString& Contact::getNickName() const
{
   return m_NickName;
}

///Get the firstname
const QString& Contact::getFirstName() const
{
   return m_FirstName;
}

///Get the second/family name
const QString& Contact::getSecondName() const
{
   return m_SecondName;
}

///Get the photo
const QPixmap* Contact::getPhoto() const
{
   return m_pPhoto;
}

///Get the formatted name
const QString& Contact::getFormattedName() const
{
   return m_FormattedName;
}

///Get the organisation
const QString& Contact::getOrganization()  const
{
   return m_Organization;
}

///Get the preferred email
const QString& Contact::getPreferredEmail()  const
{
   return m_PreferredEmail;
}

///Get the unique identifier (used for drag and drop) 
const QString& Contact::getUid() const
{
   return m_Uid;
}

///Get the group
const QString& Contact::getGroup() const
{
   return m_Group;
}

const QString& Contact::getDepartment() const
{
   return m_Department;
}

///Get the contact type
const QString& Contact::getType() const
{
   return m_Type;
}

///Set the phone number (type and number) 
void Contact::setPhoneNumbers(PhoneNumbers numbers)
{
   m_Numbers    = numbers;
}

///Set the nickname
void Contact::setNickName(const QString& name)
{
   m_NickName   = name;
}

///Set the first name
void Contact::setFirstName(const QString& name)
{
   m_FirstName  = name;
}

///Set the family name
void Contact::setFamilyName(const QString& name)
{
   m_SecondName = name;
}

///Set the Photo/Avatar
void Contact::setPhoto(QPixmap* photo)
{
   m_pPhoto = photo;
}

///Set the formatted name (display name)
void Contact::setFormattedName(const QString& name)
{
   m_FormattedName = name;
}

///Set the organisation / business
void Contact::setOrganization(const QString& name)
{
   m_Organization = name;
}

///Set the default email
void Contact::setPreferredEmail(const QString& name)
{
   m_PreferredEmail = name;
}

///Set UID
void Contact::setUid(const QString& id)
{
   m_Uid = id;
}

///Set Group
void Contact::setGroup(const QString& name)
{
   m_Group = name;
}

///Set department
void Contact::setDepartment(const QString& name)
{
   m_Department = name;
}

///Turn the contact into QString-QString hash
QHash<QString,QVariant> Contact::toHash()
{
   QHash<QString,QVariant> aContact;
   //aContact[""] = PhoneNumbers   getPhoneNumbers()    const;
   aContact[ "nickName"       ] = getNickName();
   aContact[ "firstName"      ] = getFirstName();
   aContact[ "secondName"     ] = getSecondName();
   aContact[ "formattedName"  ] = getFormattedName();
   aContact[ "organization"   ] = getOrganization();
   aContact[ "uid"            ] = getUid();
   aContact[ "preferredEmail" ] = getPreferredEmail();
   //aContact[ "Photo"          ] = QVariant(*getPhoto());
   aContact[ "type"           ] = getType();
   aContact[ "group"          ] = getGroup();
   aContact[ "department"     ] = getDepartment();
   return aContact;
}
