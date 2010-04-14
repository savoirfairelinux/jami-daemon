/***************************************************************************
 *   Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).     *
 *   All rights reserved.                                                  *
 *   Contact: Nokia Corporation (qt-info@nokia.com)                        *
 *   Author : Mathieu Leduc-Hamel mathieu.leduc-hamel@savoirfairelinux.com *
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

#include <QtCore/QStringList>

#include <klocale.h>
#include <kdebug.h>

#include "sflphone_const.h"
#include "CallTreeItem.h"

const char * CallTreeItem::callStateIcons[12] = {ICON_INCOMING, ICON_RINGING, ICON_CURRENT, ICON_DIALING, ICON_HOLD, ICON_FAILURE, ICON_BUSY, ICON_TRANSFER, ICON_TRANSF_HOLD, "", "", ICON_CONFERENCE};

CallTreeItem::CallTreeItem(QWidget *parent)
   : itemCall(0), QWidget(parent)
{
   
}

CallTreeItem::~CallTreeItem()
{
   
}

Call* CallTreeItem::call() const
{
   return itemCall;
}

void CallTreeItem::setCall(Call *call)
{
   itemCall = call;

   labelIcon = new QLabel();
   labelCallNumber2 = new QLabel(itemCall->getPeerPhoneNumber());
   labelTransferPrefix = new QLabel(i18n("Transfer to : "));
   labelTransferNumber = new QLabel();
   QSpacerItem * verticalSpacer = new QSpacerItem(16777215, 20, QSizePolicy::Expanding, QSizePolicy::Expanding);
   
   QHBoxLayout * mainLayout = new QHBoxLayout();
   mainLayout->setContentsMargins ( 3, 1, 2, 1);
   
   mainLayout->setSpacing(4);
   QVBoxLayout * descr = new QVBoxLayout();
   descr->setMargin(1);
   descr->setSpacing(1);
   QHBoxLayout * transfer = new QHBoxLayout();
   transfer->setMargin(0);
   transfer->setSpacing(0);
   mainLayout->addWidget(labelIcon);
        
   if(! itemCall->getPeerName().isEmpty()) {
      labelPeerName = new QLabel(itemCall->getPeerName());
      descr->addWidget(labelPeerName);
   }

   descr->addWidget(labelCallNumber2);
   transfer->addWidget(labelTransferPrefix);
   transfer->addWidget(labelTransferNumber);
   descr->addLayout(transfer);
   descr->addItem(verticalSpacer);
   mainLayout->addLayout(descr);
   
   setLayout(mainLayout);
   setMinimumSize(QSize(50, 30));

   connect(itemCall, SIGNAL(changed()),
           this,     SLOT(updated()));

   updated();
}

void CallTreeItem::updated()
{
   call_state state = itemCall->getState();
   bool recording = itemCall->getRecording();
   if(state != CALL_STATE_OVER) {
      if(state == CALL_STATE_CURRENT && recording) {
         labelIcon->setPixmap(QPixmap(ICON_CURRENT_REC));
      }
      else {
         QString str = QString(callStateIcons[state]);
         labelIcon->setPixmap(QPixmap(str));
      }
      bool transfer = state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD;
      labelTransferPrefix->setVisible(transfer);
      labelTransferNumber->setVisible(transfer);

      if(!transfer) {
         labelTransferNumber->setText("");
      }
      labelTransferNumber->setText(itemCall->getTransferNumber());
      labelCallNumber2->setText(itemCall->getPeerPhoneNumber());
                
      if(state == CALL_STATE_DIALING) {
         labelCallNumber2->setText(itemCall->getCallNumber());
      }
   }
   else {
      qDebug() << "Updating item of call of state OVER. Doing nothing.";
   }
}

void CallTreeItem::setConference(bool value) {
   conference = value;
}

bool CallTreeItem::isConference() {
   return conference;
}
    