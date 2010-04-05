/***************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                              *
 *   Author : Jérémy Quentin                                               *
 *   jeremy.quentin@savoirfairelinux.com                                   *
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

#include "ContactItemWidget.h"
#include "sflphone_const.h"


Contact::Contact(Addressee addressee, const PhoneNumber & number, bool displayPhoto)
{
   this->firstName = addressee.name();
   this->secondName = addressee.familyName();
   this->nickName = addressee.nickName();
   this->phoneNumber = number.number();
   this->type = number.type();
   this->displayPhoto = displayPhoto;
   if(displayPhoto) {
      this->photo = new Picture(addressee.photo());
   }
   else {
      this->photo = NULL;
   }
   
   initItem();
}


Contact::~Contact()
{
   delete item;
   delete itemWidget;
   delete photo;
}

void Contact::initItem()
{
   this->item = new QListWidgetItem();
   this->item->setSizeHint(QSize(140,CONTACT_ITEM_HEIGHT));
   initItemWidget();
}

void Contact::initItemWidget()
{
   this->itemWidget = new ContactItemWidget(this, displayPhoto);
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

const Picture * Contact::getPhoto() const
{
   return photo;
}

PhoneNumber::Type Contact::getType() const
{
   return type;
}


