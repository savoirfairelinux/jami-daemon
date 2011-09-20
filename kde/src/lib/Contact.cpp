/***************************************************************************
 *   Copyright (C) 2009-2010 by Savoir-Faire Linux                         *
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
 ***************************************************************************/
#include "Contact.h"

#include <QtCore/QDebug>

#include "sflphone_const.h"


Contact::Contact()
{
//    this->firstName = addressee.name();
//    this->secondName = addressee.familyName();
//    this->nickName = addressee.nickName();
//    this->phoneNumber = number.number();
//    this->type = number.type();
//    this->displayPhoto = displayPhoto;
//    if(displayPhoto) {
//       this->photo = new Picture(addressee.photo());
//    }
//    else {
//       this->photo = NULL;
//    }
   
   initItem();
}


Contact::~Contact()
{
   delete m_pPhoto;
}

void Contact::initItem()
{
   initItemWidget();
}

void Contact::initItemWidget()
{
   
}

PhoneNumbers Contact::getPhoneNumbers() const
{
   return m_pNumbers;
}

QString Contact::getNickName() const
{
   return m_pNickName;
}

QString Contact::getFirstName() const
{
   return m_pFirstName;
}

QString Contact::getSecondName() const
{
   return m_pSecondName;
}

const QPixmap* Contact::getPhoto() const
{
   return m_pPhoto;
}

QString Contact::getFormattedName() const
{
   return m_pFormattedName;
}

QString Contact::getOrganization()  const
{
   return m_pOrganization;
}

QString Contact::getPreferredEmail()  const
{
   return m_pPreferredEmail;
}

QString Contact::getType() const
{
   return m_pType;
}

void Contact::setPhoneNumbers(PhoneNumbers numbers)
{
   m_pNumbers    = numbers;
}

void Contact::setNickName(QString name)
{
   m_pNickName   = name;
}

void Contact::setFirstName(QString name)
{
   m_pFirstName  = name;
}

void Contact::setFamilyName(QString name)
{
   m_pSecondName = name;
}

void Contact::setPhoto(QPixmap* photo)
{
   m_pPhoto      = photo;
}

void Contact::setFormattedName(QString name)
{
   m_pFormattedName = name;
}


void Contact::setOrganization(QString name)
{
   m_pOrganization = name;
}

void Contact::setPreferredEmail(QString name)
{
   m_pPreferredEmail = name;
}