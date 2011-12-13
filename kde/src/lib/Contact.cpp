/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 **************************************************************************/

//Parent
#include "Contact.h"

//Qt
#include <QtCore/QDebug>
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
QString Contact::getNickName() const
{
   return m_NickName;
}

///Get the firstname
QString Contact::getFirstName() const
{
   return m_FirstName;
}

///Get the second/family name
QString Contact::getSecondName() const
{
   return m_SecondName;
}

///Get the photo
const QPixmap* Contact::getPhoto() const
{
   return m_pPhoto;
}

///Get the formatted name
QString Contact::getFormattedName() const
{
   return m_FormattedName;
}

///Get the organisation
QString Contact::getOrganization()  const
{
   return m_Organization;
}

///Get the preferred email
QString Contact::getPreferredEmail()  const
{
   return m_PreferredEmail;
}

///Get the unique identifier (used for drag and drop) 
QString Contact::getUid() const
{
   return m_Uid;
}

///Get the contact type
QString Contact::getType() const
{
   return m_Type;
}

///Set the phone number (type and number) 
void Contact::setPhoneNumbers(PhoneNumbers numbers)
{
   m_Numbers    = numbers;
}

///Set the nickname
void Contact::setNickName(QString name)
{
   m_NickName   = name;
}

///Set the first name
void Contact::setFirstName(QString name)
{
   m_FirstName  = name;
}

///Set the family name
void Contact::setFamilyName(QString name)
{
   m_SecondName = name;
}

///Set the Photo/Avatar
void Contact::setPhoto(QPixmap* photo)
{
   m_pPhoto      = photo;
}

///Set the formatted name (display name)
void Contact::setFormattedName(QString name)
{
   m_FormattedName = name;
}

///Set the organisation / business
void Contact::setOrganization(QString name)
{
   m_Organization = name;
}

///Set the default email
void Contact::setPreferredEmail(QString name)
{
   m_PreferredEmail = name;
}

///Set UID
void Contact::setUid(QString id)
{
   m_Uid = id;
}