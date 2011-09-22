/***************************************************************************
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
#include <unistd.h>

#include "lib/sflphone_const.h"
#include "CallTreeItem.h"
#include "lib/Contact.h"
#include "AkonadiBackend.h"

const char * CallTreeItem::callStateIcons[12] = {ICON_INCOMING, ICON_RINGING, ICON_CURRENT, ICON_DIALING, ICON_HOLD, ICON_FAILURE, ICON_BUSY, ICON_TRANSFER, ICON_TRANSF_HOLD, "", "", ICON_CONFERENCE};

CallTreeItem::CallTreeItem(QWidget *parent)
   : QWidget(parent), itemCall(0), init(false)
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
   
   if (itemCall->isConference()) {
      if (!init) {
         labelHistoryPeerName = new QLabel("Conference",this);
         labelIcon = new QLabel("Icn",this);
         QHBoxLayout* mainLayout = new QHBoxLayout();
         mainLayout->addWidget(labelIcon);
         mainLayout->addWidget(labelHistoryPeerName);
         setLayout(mainLayout);
         init = true;
      }
      labelIcon->setPixmap(QPixmap(ICON_CONFERENCE));
      labelIcon->setVisible(true);
      labelHistoryPeerName->setVisible(true);
      return;
   }

   labelIcon           = new QLabel();
   labelCallNumber2    = new QLabel(itemCall->getPeerPhoneNumber());
   labelTransferPrefix = new QLabel(i18n("Transfer to : "));
   labelTransferNumber = new QLabel();
   labelPeerName       = new QLabel();
   QSpacerItem* verticalSpacer = new QSpacerItem(16777215, 20, QSizePolicy::Expanding, QSizePolicy::Expanding);
   
   QHBoxLayout* mainLayout = new QHBoxLayout();
   mainLayout->setContentsMargins ( 3, 1, 2, 1);
   
   labelCodec = new QLabel(this);
   //labelCodec->setText("Codec: "+itemCall->getCurrentCodecName());

   labelSecure = new QLabel(this);
   
   mainLayout->setSpacing(4);
   QVBoxLayout* descr = new QVBoxLayout();
   descr->setMargin(1);
   descr->setSpacing(1);
   QHBoxLayout* transfer = new QHBoxLayout();
   transfer->setMargin(0);
   transfer->setSpacing(0);
   mainLayout->addWidget(labelIcon);
        
   if(! itemCall->getPeerName().isEmpty()) {
      labelPeerName->setText(itemCall->getPeerName());
      descr->addWidget(labelPeerName);
   }

   descr->addWidget(labelCallNumber2);
   descr->addWidget(labelSecure);
   descr->addWidget(labelCodec);
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
   Contact* contact = AkonadiBackend::getInstance()->getContactByPhone(itemCall->getPeerPhoneNumber());
   if (contact) {
      labelIcon->setPixmap(*contact->getPhoto());
      labelPeerName->setText("<b>"+contact->getFormattedName()+"</b>");
   }
   else {
      labelIcon->setPixmap(QPixmap(KIcon("user-identity").pixmap(QSize(48,48))));

      if(! itemCall->getPeerName().trimmed().isEmpty()) {
         labelPeerName->setText("<b>"+itemCall->getPeerName()+"</b>");
      }
      else {
         labelPeerName->setText("<b>Unknow</b>");
      }
   }
      
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
      else {
         labelCodec->setText("Codec: "+itemCall->getCurrentCodecName());
         if (itemCall->isSecure())
            labelSecure->setText("⚷");
      }
   }
   else {
      qDebug() << "Updating item of call of state OVER. Doing nothing.";
   }
   
}
    
