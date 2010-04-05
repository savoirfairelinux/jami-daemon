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
#include "ContactItemWidget.h"

#include <QtCore/QString>
#include <QtGui/QVBoxLayout>
#include <QtCore/QDebug>
#include <QtGui/QSpacerItem>

#include "sflphone_const.h"

ContactItemWidget::ContactItemWidget(const Contact * contact, bool displayPhoto, QWidget *parent)
 : QWidget(parent)
{
   if(!contact->getNickName().isEmpty()) {
      contactName = new QLabel(contact->getNickName());
   }
   else {
      contactName = new QLabel(contact->getFirstName());
   }
   if(displayPhoto) {
      if(!contact->getPhoto()->isEmpty()) {
         QPixmap pixmap;
         if(contact->getPhoto()->isIntern()) {
            contactPhoto = new QLabel();
            pixmap = QPixmap::fromImage(contact->getPhoto()->data());
         }
         else {
            contactPhoto = new QLabel();
            pixmap = QPixmap(contact->getPhoto()->url());
         }
         if(pixmap.height() > pixmap.width())
            contactPhoto->setPixmap(pixmap.scaledToHeight(CONTACT_ITEM_HEIGHT-4));
         else
            contactPhoto->setPixmap(pixmap.scaledToWidth(CONTACT_ITEM_HEIGHT-4));
      }
      else {
         contactPhoto = new QLabel();
         contactPhoto->setMinimumSize(CONTACT_ITEM_HEIGHT-4, 0);
      }
   }
   contactType = new QLabel(PhoneNumber::typeLabel(contact->getType()));
   contactNumber = new QLabel(contact->getPhoneNumber());
   QSpacerItem * horizontalSpacer = new QSpacerItem(16777215, 20, QSizePolicy::Preferred, QSizePolicy::Minimum);
   QHBoxLayout * hlayout = new QHBoxLayout(this);
   QVBoxLayout * vlayout = new QVBoxLayout();
   hlayout->setMargin(1);
   hlayout->setSpacing(4);
   vlayout->setMargin(1);
   vlayout->setSpacing(2);
   vlayout->addWidget(contactName);
   vlayout->addWidget(contactNumber);
   if(displayPhoto) {
      hlayout->addWidget(contactPhoto);
   }
   hlayout->addLayout(vlayout);
   hlayout->addItem(horizontalSpacer);
   hlayout->addWidget(contactType);
   this->setLayout(hlayout);
}


ContactItemWidget::~ContactItemWidget()
{
   delete contactName;
   delete contactNumber;
//    delete contactPhoto;
   delete contactType;
}


QString ContactItemWidget::getContactName()
{
   return contactName->text();
}

QString ContactItemWidget::getContactNumber()
{
   return contactNumber->text();
}

