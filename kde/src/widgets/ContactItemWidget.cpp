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
#include "ContactItemWidget.h"

//Qt
#include <QtCore/QMimeData>
#include <QtGui/QApplication>
#include <QtGui/QClipboard>
#include <QtGui/QGridLayout>
#include <QtGui/QMenu>
#include <QtGui/QLabel>
#include <QtGui/QSpacerItem>
#include <QtGui/QInputDialog>

//KDE
#include <KIcon>
#include <KLocale>
#include <KDebug>
#include <KAction>
#include <KStandardDirs>

//System
#include <unistd.h>

//SFLPhone
#include "klib/AkonadiBackend.h"
#include "widgets/BookmarkDock.h"
#include "klib/ConfigurationSkeleton.h"
#include "widgets/TranslucentButtons.h"
#include "SFLPhone.h"

//SFLPhone library
#include "lib/Contact.h"
#include "lib/sflphone_const.h"

///Constructor
ContactItemWidget::ContactItemWidget(QWidget *parent)
   : QWidget(parent), m_pMenu(0),m_pOrganizationL(0),m_pEmailL(0),m_pContactKA(0), m_pIconL(0), m_pContactNameL(0),
   m_pCallNumberL(0)
{
   setContextMenuPolicy(Qt::CustomContextMenu);
   setAcceptDrops(true);
   
   m_pCallAgain  = new KAction(this);
   m_pCallAgain->setShortcut   ( Qt::CTRL + Qt::Key_Enter   );
   m_pCallAgain->setText       ( i18n("Call Again")         );
   m_pCallAgain->setIcon       ( KIcon("call-start")        );

   m_pEditContact = new KAction(this);
   m_pEditContact->setShortcut ( Qt::CTRL + Qt::Key_E       );
   m_pEditContact->setText     ( i18n("Edit contact")       );
   m_pEditContact->setIcon     ( KIcon("contact-new")       );

   m_pCopy       = new KAction(this);
   m_pCopy->setShortcut        ( Qt::CTRL + Qt::Key_C       );
   m_pCopy->setText            ( i18n("Copy")               );
   m_pCopy->setIcon            ( KIcon("edit-copy")         );

   m_pEmail      = new KAction(this);
   m_pEmail->setShortcut       ( Qt::CTRL + Qt::Key_M       );
   m_pEmail->setText           ( i18n("Send Email")         );
   m_pEmail->setIcon           ( KIcon("mail-message-new")  );

   m_pAddPhone      = new KAction(this);
   m_pAddPhone->setShortcut    ( Qt::CTRL + Qt::Key_N       );
   m_pAddPhone->setText        ( i18n("Add Phone Number")   );
   m_pAddPhone->setIcon        ( KIcon("list-resource-add") );

   m_pBookmark      = new KAction(this);
   m_pBookmark->setShortcut    ( Qt::CTRL + Qt::Key_D       );
   m_pBookmark->setText        ( i18n("Bookmark")           );
   m_pBookmark->setIcon        ( KIcon("bookmarks")         );

   connect(m_pCallAgain    , SIGNAL(triggered()) , this,SLOT(callAgain()      ));
   connect(m_pEditContact  , SIGNAL(triggered()) , this,SLOT(editContact()    ));
   connect(m_pCopy         , SIGNAL(triggered()) , this,SLOT(copy()           ));
   connect(m_pEmail        , SIGNAL(triggered()) , this,SLOT(sendEmail()      ));
   connect(m_pAddPhone     , SIGNAL(triggered()) , this,SLOT(addPhone()       ));
   connect(m_pBookmark     , SIGNAL(triggered()) , this,SLOT(bookmark()       ));

   //Overlay
   m_pBtnTrans = new TranslucentButtons(this);
   m_pBtnTrans->setText(i18n("Transfer"));
   m_pBtnTrans->setVisible(false);
   m_pBtnTrans->setPixmap(new QImage(KStandardDirs::locate("data","sflphone-client-kde/transferarraw.png")));
   connect(m_pBtnTrans,SIGNAL(dataDropped(QMimeData*)),this,SLOT(transferEvent(QMimeData*)));
} //ContactItemWidget

///Destructor
ContactItemWidget::~ContactItemWidget()
{
   /*delete m_pIconL        ;
   delete m_pContactNameL ;
   delete m_pCallNumberL  ;
   delete m_pOrganizationL;
   delete m_pEmailL       ;
   delete m_pItem         ;
   
   delete m_pCallAgain   ;
   delete m_pEditContact ;
   delete m_pCopy        ;
   delete m_pEmail       ;
   delete m_pAddPhone    ;
   delete m_pBookmark    ;
   delete m_pMenu        ;*/
}


/*****************************************************************************
 *                                                                           *
 *                                  Setters                                  *
 *                                                                           *
 ****************************************************************************/

///Set the contact
void ContactItemWidget::setContact(Contact* contact)
{
   m_pContactKA     = contact;
   m_pIconL         = new QLabel ( this );
   m_pContactNameL  = new QLabel (      );
   m_pCallNumberL   = new QLabel ( this );

   m_pIconL->setMinimumSize(70,48);
   m_pIconL->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

   QSpacerItem* verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);

   m_pIconL->setMaximumSize(48,9999);
   m_pIconL->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

   m_pIconL->setPixmap(QPixmap(KIcon("user-identity").pixmap(QSize(48,48))));

   QGridLayout* mainLayout = new QGridLayout(this);
   mainLayout->setContentsMargins(0,0,0,0);
   mainLayout->addWidget( m_pIconL        , 0 , 0 , 4 , 1 );
   mainLayout->addWidget( m_pContactNameL , 0 , 1         );

   uint row = 1;

   if (ConfigurationSkeleton::displayOrganisation() && !contact->getOrganization().isEmpty()) {
      m_pOrganizationL = new QLabel ( this );
      mainLayout->addWidget( m_pOrganizationL, row , 1);
      row++;
   }
   mainLayout->addWidget( m_pCallNumberL  , row , 1       );
   row++;

   if (ConfigurationSkeleton::displayEmail() && !contact->getPreferredEmail().isEmpty()) {
      m_pEmailL        = new QLabel (      );
      mainLayout->addWidget( m_pEmailL       , row , 1    );
      row++;
   }
   
   mainLayout->addItem(verticalSpacer     , row , 1       );

   setLayout(mainLayout);
   setMinimumSize(QSize(50, 30));

   updated();
   connect(this,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(showContext(QPoint)));

   uint height =0;
   if ( m_pContactNameL  ) {
      QFontMetrics fm(m_pContactNameL->font());
      height += fm.height();
   }
   if ( m_pCallNumberL   ) {
      QFontMetrics fm(m_pCallNumberL->font());
      height += fm.height();
   }
   if ( m_pOrganizationL ) {
      QFontMetrics fm(m_pOrganizationL->font());
      height += fm.height();
   }
   if ( m_pEmailL        ) {
      QFontMetrics fm(m_pEmailL->font());
      height += fm.height();
   }

   if (height < 48)
      height = 48;
   m_Size = QSize(0,height+8);
} //setContact

///Set the model index
void ContactItemWidget::setItem(QTreeWidgetItem* item)
{
   m_pItem = item;
}


/*****************************************************************************
 *                                                                           *
 *                                    Slots                                  *
 *                                                                           *
 ****************************************************************************/

///The contact need to be updated
void ContactItemWidget::updated()
{
   m_pContactNameL->setText("<b>"+m_pContactKA->getFormattedName()+"</b>");
   if (m_pOrganizationL && !m_pContactKA->getOrganization().isEmpty()) {
      m_pOrganizationL->setText(m_pContactKA->getOrganization());
   }
   else if (m_pOrganizationL) {
      m_pOrganizationL->setVisible(false);
   }

   if (m_pEmailL && !getEmail().isEmpty()) {
      m_pEmailL->setText(getEmail());
   }
   else if (m_pEmailL) {
      m_pEmailL->setVisible(false);
   }

   PhoneNumbers numbers = m_pContactKA->getPhoneNumbers();

   if (getCallNumbers().count() == 1)
      m_pCallNumberL->setText(getCallNumbers()[0]->getNumber());
   else
      m_pCallNumberL->setText(QString::number(getCallNumbers().count())+i18n(" numbers"));

   if (!m_pContactKA->getPhoto())
      m_pIconL->setPixmap(QPixmap(KIcon("user-identity").pixmap(QSize(48,48))));
   else
      m_pIconL->setPixmap(*m_pContactKA->getPhoto());
} //updated


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///Return contact name
const QString& ContactItemWidget::getContactName() const
{
   return m_pContactKA->getFormattedName();
}

///Return call number
PhoneNumbers ContactItemWidget::getCallNumbers() const
{
   return m_pContactKA->getPhoneNumbers();
}

///Return the organisation
const QString& ContactItemWidget::getOrganization() const
{
   return m_pContactKA->getOrganization();
}

///Return the email address
const QString& ContactItemWidget::getEmail() const
{
   return m_pContactKA->getPreferredEmail();
}

///Return the picture
QPixmap* ContactItemWidget::getPicture() const
{
   return (QPixmap*) m_pContactKA->getPhoto();
}

///Return the model index
QTreeWidgetItem* ContactItemWidget::getItem() const
{
   return m_pItem;
}

///Return the contact object
Contact* ContactItemWidget::getContact() const
{
   return m_pContactKA;
}

///Select a number
QString ContactItemWidget::showNumberSelector(bool& ok)
{
   if (m_pContactKA->getPhoneNumbers().size() > 1) {
      QStringList list;
      QHash<QString,QString> map;
      foreach (Contact::PhoneNumber* number, m_pContactKA->getPhoneNumbers()) {
         map[number->getType()+" ("+number->getNumber()+")"] = number->getNumber();
         list << number->getType()+" ("+number->getNumber()+")";
      }
      QString result = QInputDialog::getItem (this, i18n("Select phone number"), i18n("This contact have many phone number, please select the one you wish to call"), list, 0, false, &ok);

      if (!ok) {
         kDebug() << "Operation cancelled";
      }
      return map[result];
   }
   else if (m_pContactKA->getPhoneNumbers().size() == 1) {
      ok = true;
      return m_pContactKA->getPhoneNumbers()[0]->getNumber();
   }
   else {
      ok = false;
      return "";
   }
}

///Return precalculated size hint, prevent it from being computed over and over
QSize ContactItemWidget::sizeHint () const
{
   return m_Size;
}

/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Show the context menu
void ContactItemWidget::showContext(const QPoint& pos)
{
   if (!m_pMenu) {
      m_pMenu = new QMenu( this          );
      m_pMenu->addAction( m_pCallAgain   );
      m_pMenu->addAction( m_pEditContact );
      m_pMenu->addAction( m_pAddPhone    );
      m_pMenu->addAction( m_pCopy        );
      m_pMenu->addAction( m_pEmail       );
      m_pMenu->addAction( m_pBookmark    );
   }
   PhoneNumbers numbers = m_pContactKA->getPhoneNumbers();
   m_pBookmark->setEnabled(numbers.count() == 1);
   m_pMenu->exec(mapToGlobal(pos));
} //showContext

///Send an email
//TODO
void ContactItemWidget::sendEmail()
{
   kDebug() << "Sending email";
}

///Call the same number again
//TODO
void ContactItemWidget::callAgain()
{
   kDebug() << "Calling ";
   bool ok;
   QString number = showNumberSelector(ok);
   if (ok) {
      Call* call = SFLPhone::model()->addDialingCall(m_pContactKA->getFormattedName(), AccountList::getCurrentAccount());
      call->setCallNumber(number);
      call->setPeerName(m_pContactKA->getFormattedName());
      call->actionPerformed(CALL_ACTION_ACCEPT);
   }
}

///Copy contact to clipboard
void ContactItemWidget::copy()
{
   kDebug() << "Copying contact";
   QMimeData* mimeData = new QMimeData();
   mimeData->setData(MIME_CONTACT, m_pContactKA->getUid().toUtf8());
   QString numbers(m_pContactKA->getFormattedName()+": ");
   QString numbersHtml("<b>"+m_pContactKA->getFormattedName()+"</b><br />");
   foreach (Contact::PhoneNumber* number, m_pContactKA->getPhoneNumbers()) {
      numbers     += number->getNumber()+" ("+number->getType()+")  ";
      numbersHtml += number->getNumber()+" ("+number->getType()+")  <br />";
   }
   mimeData->setData("text/plain", numbers.toUtf8());
   mimeData->setData("text/html", numbersHtml.toUtf8());
   QClipboard* clipboard = QApplication::clipboard();
   clipboard->setMimeData(mimeData);
}

///Edit this contact
void ContactItemWidget::editContact()
{
   kDebug() << "Edit contact";
   AkonadiBackend::getInstance()->editContact(m_pContactKA);
}

///Add a new phone number for this contact
//TODO
void ContactItemWidget::addPhone()
{
   kDebug() << "Adding to contact";
}

///Add this contact to the bookmark list
void ContactItemWidget::bookmark()
{
   PhoneNumbers numbers = m_pContactKA->getPhoneNumbers();
   if (numbers.count() == 1)
      SFLPhone::app()->bookmarkDock()->addBookmark(numbers[0]->getNumber());
}


/*****************************************************************************
 *                                                                           *
 *                                 Drag&Dop                                  *
 *                                                                           *
 ****************************************************************************/

///Called when a drag and drop occure while the item have not been dropped yet
void ContactItemWidget::dragEnterEvent ( QDragEnterEvent *e )
{
   kDebug() << "Drag enter";
   if (e->mimeData()->hasFormat( MIME_CALLID) && m_pBtnTrans) {
      m_pBtnTrans->setHoverState(true);
      m_pBtnTrans->setMinimumSize(width()-16,height()-4);
      m_pBtnTrans->setMaximumSize(width()-16,height()-4);
      m_pBtnTrans->move(8,2);
      m_pBtnTrans->setVisible(true);
      m_pBtnTrans->setHoverState(true);
      e->accept();
   }
   else
      e->ignore();
} //dragEnterEvent

///The cursor move on a potential drag event
void ContactItemWidget::dragMoveEvent  ( QDragMoveEvent  *e )
{
   m_pBtnTrans->setHoverState(true);
   e->accept();
}

///A potential drag event is cancelled
void ContactItemWidget::dragLeaveEvent ( QDragLeaveEvent *e )
{
   m_pBtnTrans->setHoverState(false);
   m_pBtnTrans->setVisible(false);
   kDebug() << "Drag leave";
   e->accept();
}

///Called when a call is dropped on transfer
void ContactItemWidget::transferEvent(QMimeData* data)
{
   if (data->hasFormat( MIME_CALLID)) {
      bool ok;
      QString result = showNumberSelector(ok);
      if (ok) {
         Call* call = SFLPhone::model()->getCall(data->data(MIME_CALLID));
         if (dynamic_cast<Call*>(call)) {
            call->changeCurrentState(CALL_STATE_TRANSFER);
            SFLPhone::model()->transfer(call, result);
         }
      }
   }
   else
      kDebug() << "Invalid mime data";
   m_pBtnTrans->setHoverState(false);
   m_pBtnTrans->setVisible(false);
}

///On data drop
void ContactItemWidget::dropEvent(QDropEvent *e)
{
   kDebug() << "Drop accepted";
   if (dynamic_cast<const QMimeData*>(e->mimeData()) && e->mimeData()->hasFormat( MIME_CALLID)) {
      transferEvent((QMimeData*)e->mimeData());
      e->accept();
   }
   else {
      kDebug() << "Invalid drop data";
      e->ignore();
   }
}