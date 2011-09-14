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
   delete photo;
}

void Contact::initItem()
{
   initItemWidget();
}

void Contact::initItemWidget()
{
   
}

QString Contact::getPhoneNumber() const
{
   return phoneNumber;
}

QString Contact::getNickName() const
{
   return nickName;
}

QString Contact::getFirstName() const
{
   return firstName;
}

QString Contact::getSecondName() const
{
   return secondName;
}

const QPixmap* Contact::getPhoto() const
{
   return photo;
}

QString Contact::getType() const
{
   return type;
}


