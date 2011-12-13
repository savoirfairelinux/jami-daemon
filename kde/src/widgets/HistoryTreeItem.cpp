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
 **************************************************************************/

//Parent
#include "HistoryTreeItem.h"

//Qt
#include <QtCore/QStringList>
#include <QtGui/QGridLayout>
#include <QtGui/QMenu>
#include <QtGui/QLabel>
#include <QtGui/QSpacerItem>

//KDE
#include <KLocale>
#include <KDebug>
#include <KAction>
#include <KIcon>

//SFLPhone library
#include "lib/sflphone_const.h"
#include "lib/Contact.h"
#include "lib/Call.h"

//SFLPhone
#include "AkonadiBackend.h"
#include "SFLPhone.h"
#include "widgets/BookmarkDock.h"

const char * HistoryTreeItem::callStateIcons[12] = {ICON_INCOMING, ICON_RINGING, ICON_CURRENT, ICON_DIALING, ICON_HOLD, ICON_FAILURE, ICON_BUSY, ICON_TRANSFER, ICON_TRANSF_HOLD, "", "", ICON_CONFERENCE};

///Constructor
HistoryTreeItem::HistoryTreeItem(QWidget *parent ,QString phone)
   : QWidget(parent), m_pItemCall(0),m_pMenu(0)
{
   setContextMenuPolicy(Qt::CustomContextMenu);

   m_pCallAgain    = new KAction(this);
   m_pAddContact   = new KAction(this);
   m_pCopy         = new KAction(this);
   m_pEmail        = new KAction(this);
   m_pAddToContact = new KAction(this);
   m_pBookmark     = new KAction(this);
   
   m_pCallAgain->setShortcut    ( Qt::CTRL + Qt::Key_Enter       );
   m_pCallAgain->setText        ( i18n("Call Again")             );
   m_pCallAgain->setIcon        ( KIcon(ICON_DIALING)            );

   m_pAddToContact->setShortcut ( Qt::CTRL + Qt::Key_E           );
   m_pAddToContact->setText     ( i18n("Add Number to Contact")  );
   m_pAddToContact->setIcon     ( KIcon("list-resource-add")     );
   m_pAddToContact->setDisabled ( true                           );
   
   m_pAddContact->setShortcut   ( Qt::CTRL + Qt::Key_E           );
   m_pAddContact->setText       ( i18n("Add Contact")            );
   m_pAddContact->setIcon       ( KIcon("contact-new")           );
   
   m_pCopy->setShortcut         ( Qt::CTRL + Qt::Key_C           );
   m_pCopy->setText             ( i18n("Copy")                   );
   m_pCopy->setIcon             ( KIcon("edit-copy")             );
   m_pCopy->setDisabled         ( true                           );
   
   m_pEmail->setShortcut        ( Qt::CTRL + Qt::Key_M           );
   m_pEmail->setText            ( i18n("Send Email")             );
   m_pEmail->setIcon            ( KIcon("mail-message-new")      );
   m_pEmail->setDisabled        ( true                           );

   m_pBookmark->setShortcut     ( Qt::CTRL + Qt::Key_D           );
   m_pBookmark->setText         ( i18n("Bookmark")               );
   m_pBookmark->setIcon         ( KIcon("bookmarks")             );

   connect(m_pCallAgain    , SIGNAL(triggered())                        , this , SLOT(callAgain()         ));
   connect(m_pAddContact   , SIGNAL(triggered())                        , this , SLOT(addContact()        ));
   connect(m_pCopy         , SIGNAL(triggered())                        , this , SLOT(copy()              ));
   connect(m_pEmail        , SIGNAL(triggered())                        , this , SLOT(sendEmail()         ));
   connect(m_pAddToContact , SIGNAL(triggered())                        , this , SLOT(addToContact()      ));
   connect(m_pBookmark     , SIGNAL(triggered())                        , this , SLOT(bookmark()          ));
   connect(this            , SIGNAL(customContextMenuRequested(QPoint)) , this , SLOT(showContext(QPoint) ));

   m_pIconL         = new QLabel( this );
   m_pPeerNameL     = new QLabel( this );
   m_pCallNumberL   = new QLabel( this );
   m_pDurationL     = new QLabel( this );
   m_pTimeL         = new QLabel( this );
   
   m_pIconL->setMinimumSize(70,48);
   m_pIconL->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
   QSpacerItem* verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);

   QGridLayout* mainLayout = new QGridLayout(this);
   mainLayout->addWidget ( m_pIconL,0,0,4,1     );
   mainLayout->addWidget ( m_pPeerNameL,0,1     );
   mainLayout->addWidget ( m_pCallNumberL,1,1   );
   mainLayout->addWidget ( m_pTimeL,2,1         );
   mainLayout->addItem   ( verticalSpacer,3,1   );
   mainLayout->addWidget ( m_pDurationL,0,2,4,1 );
   setLayout(mainLayout);
   setMinimumSize(QSize(50, 30));

   if (!phone.isEmpty()) {
      getContactInfo(phone);
      m_pCallNumberL->setText(phone);
      m_PhoneNumber = phone;
   }
}

///Destructor
HistoryTreeItem::~HistoryTreeItem()
{
   
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Return the call item
Call* HistoryTreeItem::call() const
{
   return m_pItemCall;
}

///The item have to be updated
void HistoryTreeItem::updated()
{
   if (!getContactInfo(m_pItemCall->getPeerPhoneNumber())) {
      if(! m_pItemCall->getPeerName().trimmed().isEmpty()) {
         m_pPeerNameL->setText("<b>"+m_pItemCall->getPeerName()+"</b>");
      }
   }
   call_state state = m_pItemCall->getState();
   bool recording = m_pItemCall->getRecording();
   if(state != CALL_STATE_OVER) {
      if(state == CALL_STATE_CURRENT && recording) {
         m_pIconL->setPixmap(QPixmap(ICON_CURRENT_REC));
      }
      else {
         QString str = QString(callStateIcons[state]);
         m_pIconL->setPixmap(QPixmap(str));
      }
      m_pCallNumberL->setText(m_pItemCall->getPeerPhoneNumber());
                
      if(state == CALL_STATE_DIALING) {
         m_pCallNumberL->setText(m_pItemCall->getCallNumber());
      }
   }
   
}

///Show the context menu
void HistoryTreeItem::showContext(const QPoint& pos)
{
   if (!m_pMenu) {
      m_pMenu = new QMenu(this);
      m_pMenu->addAction( m_pCallAgain    );
      m_pMenu->addAction( m_pAddContact   );
      m_pMenu->addAction( m_pAddToContact );
      m_pMenu->addAction( m_pCopy         );
      m_pMenu->addAction( m_pEmail        );
      m_pMenu->addAction( m_pBookmark     );
   }
   m_pMenu->exec(mapToGlobal(pos));
}

///Send an email
void HistoryTreeItem::sendEmail()
{
   //TODO
   qDebug() << "Sending email";
}

///Call the caller again
void HistoryTreeItem::callAgain()
{
   if (m_pItemCall) {
      qDebug() << "Calling "<< m_pItemCall->getPeerPhoneNumber();
   }
   SFLPhone::model()->addDialingCall(m_Name, SFLPhone::app()->model()->getCurrentAccountId())->setCallNumber(m_PhoneNumber);
}

///Copy the call
void HistoryTreeItem::copy()
{
   //TODO
   qDebug() << "Copying contact";
}

///Create a contact from those informations
void HistoryTreeItem::addContact()
{
   qDebug() << "Adding contact";
   Contact* aContact = new Contact();
   aContact->setPhoneNumbers(PhoneNumbers() << new Contact::PhoneNumber(m_PhoneNumber, "Home"));
   aContact->setFormattedName(m_Name);
   AkonadiBackend::getInstance()->addNewContact(aContact);
}

///Add this call number to an existing contact
void HistoryTreeItem::addToContact()
{
   //TODO
   qDebug() << "Adding to contact";
}

///Bookmark this contact
void HistoryTreeItem::bookmark()
{
   qDebug() << "bookmark";
   SFLPhone::app()->bookmarkDock()->addBookmark(m_PhoneNumber);
}


/*****************************************************************************
 *                                                                           *
 *                                  Setters                                  *
 *                                                                           *
 ****************************************************************************/

///Set the call to be handled by this item
void HistoryTreeItem::setCall(Call *call)
{
   m_pItemCall = call;
   
   if (m_pItemCall->isConference()) {
      m_pIconL->setVisible(true);
      return;
   }
   
   m_pCallNumberL->setText(m_pItemCall->getPeerPhoneNumber());

   m_pTimeL->setText(QDateTime::fromTime_t(m_pItemCall->getStartTimeStamp().toUInt()).toString());

   int dur = m_pItemCall->getStopTimeStamp().toInt() - m_pItemCall->getStartTimeStamp().toInt();
   m_pDurationL->setText(QString("%1").arg(dur/3600,2)+":"+QString("%1").arg((dur%3600)/60,2)+":"+QString("%1").arg((dur%3600)%60,2)+" ");

   connect(m_pItemCall , SIGNAL(changed())                          , this , SLOT(updated()           ));
   updated();

   m_TimeStamp   = m_pItemCall->getStartTimeStamp().toUInt();
   m_Duration    = dur;
   m_Name        = m_pItemCall->getPeerName();
   m_PhoneNumber = m_pItemCall->getPeerPhoneNumber();
}

///Set the index associed with this widget
void HistoryTreeItem::setItem(QTreeWidgetItem* item)
{
   m_pItem = item;
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///Can a contact be associed with this call?
bool HistoryTreeItem::getContactInfo(QString phoneNumber)
{
   Contact* contact = AkonadiBackend::getInstance()->getContactByPhone(phoneNumber);
   if (contact) {
      if (contact->getPhoto() != NULL)
         m_pIconL->setPixmap(*contact->getPhoto());
      m_pPeerNameL->setText("<b>"+contact->getFormattedName()+"</b>");
   }
   else {
      m_pIconL->setPixmap(QPixmap(KIcon("user-identity").pixmap(QSize(48,48))));
      m_pPeerNameL->setText(i18n("<b>Unknow</b>"));
      return false;
   }
   return true;
}

///Return the time stamp
uint HistoryTreeItem::getTimeStamp()
{
   return m_TimeStamp;
}

///Return the duration
uint HistoryTreeItem::getDuration()
{
   return m_Duration;
}

///Return the caller name
QString HistoryTreeItem::getName()
{
   return m_Name;
}

///Return the caller peer number
QString HistoryTreeItem::getPhoneNumber()
{
   return m_PhoneNumber;
}

///Get the index item assiciated with this widget
QTreeWidgetItem* HistoryTreeItem::getItem()
{
   return m_pItem;
}