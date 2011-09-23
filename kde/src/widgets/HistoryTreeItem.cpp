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
#include <QtGui/QGridLayout>
#include <QtGui/QMenu>

#include <klocale.h>
#include <kdebug.h>
#include <unistd.h>
#include <kaction.h>
#include <kicon.h>

#include "lib/sflphone_const.h"
#include "HistoryTreeItem.h"
#include "AkonadiBackend.h"
#include "lib/Contact.h"
#include "SFLPhone.h"

const char * HistoryTreeItem::callStateIcons[12] = {ICON_INCOMING, ICON_RINGING, ICON_CURRENT, ICON_DIALING, ICON_HOLD, ICON_FAILURE, ICON_BUSY, ICON_TRANSFER, ICON_TRANSF_HOLD, "", "", ICON_CONFERENCE};

HistoryTreeItem::HistoryTreeItem(QWidget *parent)
   : QWidget(parent), itemCall(0),m_pMenu(0), init(false)
{
   setContextMenuPolicy(Qt::CustomContextMenu);

   m_pCallAgain    = new KAction(this);
   m_pAddContact   = new KAction(this);
   m_pCopy         = new KAction(this);
   m_pEmail        = new KAction(this);
   m_pAddToContact = new KAction(this);
   
   m_pCallAgain->setShortcut    (Qt::CTRL + Qt::Key_Enter     );
   m_pCallAgain->setText        ("Call Again"                 );
   m_pCallAgain->setIcon        (KIcon(ICON_DIALING)          );

   m_pAddToContact->setShortcut (Qt::CTRL + Qt::Key_E         );
   m_pAddToContact->setText     ("Add Number to Contact"      );
   m_pAddToContact->setIcon     (KIcon("list-resource-add")   );
   
   m_pAddContact->setShortcut   (Qt::CTRL + Qt::Key_E         );
   m_pAddContact->setText       ("Add Contact"             );
   m_pAddContact->setIcon       (KIcon("contact-new")         );
   
   m_pCopy->setShortcut         (Qt::CTRL + Qt::Key_C         );
   m_pCopy->setText             ("Copy"                       );
   m_pCopy->setIcon             (KIcon("edit-copy")           );
   
   m_pEmail->setShortcut        (Qt::CTRL + Qt::Key_M         );
   m_pEmail->setText            ("Send Email"                 );
   m_pEmail->setIcon            (KIcon("mail-message-new")    );

   connect(m_pCallAgain    ,SIGNAL(triggered()),this,SLOT(callAgain()      ));
   connect(m_pAddContact   ,SIGNAL(triggered()),this,SLOT(addContact()     ));
   connect(m_pCopy         ,SIGNAL(triggered()),this,SLOT(copy()           ));
   connect(m_pEmail        ,SIGNAL(triggered()),this,SLOT(sendEmail()      ));
   connect(m_pAddToContact ,SIGNAL(triggered()),this,SLOT(addToContact()   ));
}

HistoryTreeItem::~HistoryTreeItem()
{
   
}

Call* HistoryTreeItem::call() const
{
   return itemCall;
}

void HistoryTreeItem::setCall(Call *call)
{
   itemCall = call;
   
   if (itemCall->isConference()) {
      labelIcon->setVisible(true);
      return;
   }
   
   labelIcon     = new QLabel(this);
   labelPeerName = new QLabel();
   labelIcon     = new QLabel();
   
   labelIcon->setMinimumSize(70,48);
   labelIcon->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
   
   labelCallNumber2 = new QLabel(itemCall->getPeerPhoneNumber());
   QSpacerItem* verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);

   m_pTimeL = new QLabel();
   m_pTimeL->setText(QDateTime::fromTime_t(itemCall->getStartTimeStamp().toUInt()).toString());

   m_pDurationL = new QLabel();
   int dur = itemCall->getStopTimeStamp().toInt() - itemCall->getStartTimeStamp().toInt();
   m_pDurationL->setText(QString("%1").arg(dur/3600,2)+":"+QString("%1").arg((dur%3600)/60,2)+":"+QString("%1").arg((dur%3600)%60,2)+" ");

   QGridLayout* mainLayout = new QGridLayout(this);
   mainLayout->addWidget( labelIcon,0,0,4,1    );
   mainLayout->addWidget( labelPeerName,0,1    );
   mainLayout->addWidget( labelCallNumber2,1,1 );
   mainLayout->addWidget( m_pTimeL,2,1         );
   mainLayout->addItem  ( verticalSpacer,3,1   );
   mainLayout->addWidget( m_pDurationL,0,2,4,1 );
   
   setLayout(mainLayout);
   setMinimumSize(QSize(50, 30));

   connect(itemCall, SIGNAL(changed()), this,     SLOT(updated()));
   connect(this,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(showContext(QPoint)));
   updated();

   m_pTimeStamp = itemCall->getStartTimeStamp().toUInt();
   m_pDuration = dur;
   m_pName = itemCall->getPeerName();
   m_pPhoneNumber = itemCall->getPeerPhoneNumber();
}

void HistoryTreeItem::updated()
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
      labelCallNumber2->setText(itemCall->getPeerPhoneNumber());
                
      if(state == CALL_STATE_DIALING) {
         labelCallNumber2->setText(itemCall->getCallNumber());
      }
   }
   
}

uint HistoryTreeItem::getTimeStamp()
{
   return m_pTimeStamp;
}

uint HistoryTreeItem::getDuration()
{
   return m_pDuration;
}

QString HistoryTreeItem::getName()
{
   return m_pName;
}

QString HistoryTreeItem::getPhoneNumber()
{
   return m_pPhoneNumber;
}


QTreeWidgetItem* HistoryTreeItem::getItem()
{
   return m_pItem;
}

void HistoryTreeItem::setItem(QTreeWidgetItem* item)
{
   m_pItem = item;
}

void HistoryTreeItem::showContext(const QPoint& pos)
{
   if (!m_pMenu) {
      m_pMenu = new QMenu(this);
      m_pMenu->addAction( m_pCallAgain    );
      m_pMenu->addAction( m_pAddContact   );
      m_pMenu->addAction( m_pAddToContact );
      m_pMenu->addAction( m_pCopy         );
      m_pMenu->addAction( m_pEmail        );
   }
   m_pMenu->exec(mapToGlobal(pos));
}


void HistoryTreeItem::sendEmail()
{
   qDebug() << "Sending email";
}

void HistoryTreeItem::callAgain()
{
   qDebug() << "Calling "<< itemCall->getPeerPhoneNumber();
   SFLPhone::app()->model()->addDialingCall(m_pName, SFLPhone::app()->model()->getCurrentAccountId())->setCallNumber(m_pPhoneNumber);
   
}

void HistoryTreeItem::copy()
{
   qDebug() << "Copying contact";
}

void HistoryTreeItem::addContact()
{
   qDebug() << "Adding contact";
   Contact* aContact = new Contact();
   aContact->setPhoneNumbers(PhoneNumbers() << new Contact::PhoneNumber(m_pPhoneNumber, "Home"));
   aContact->setFormattedName(m_pName);
   AkonadiBackend::getInstance()->addNewContact(aContact);
}

void HistoryTreeItem::addToContact()
{
   qDebug() << "Adding to contact";
}
