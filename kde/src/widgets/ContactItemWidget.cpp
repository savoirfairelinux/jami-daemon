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

//KDE
#include <KIcon>
#include <KLocale>
#include <KDebug>
#include <KAction>

//System
#include <unistd.h>

//SFLPhone
#include "AkonadiBackend.h"
#include "widgets/BookmarkDock.h"
#include "SFLPhone.h"

//SFLPhone library
#include "lib/Contact.h"
#include "lib/sflphone_const.h"

///Constructor
ContactItemWidget::ContactItemWidget(QWidget *parent)
   : QWidget(parent), m_pMenu(0)
{
   setContextMenuPolicy(Qt::CustomContextMenu);
   m_pCallAgain  = new KAction(this);
   m_pCallAgain->setShortcut   ( Qt::CTRL + Qt::Key_Enter   );
   m_pCallAgain->setText       ( i18n("Call Again")         );
   m_pCallAgain->setIcon       ( KIcon(ICON_DIALING)        );
   
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
}

///Destructor
ContactItemWidget::~ContactItemWidget()
{

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
   m_pOrganizationL = new QLabel ( this );
   m_pEmailL        = new QLabel (      );
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
   mainLayout->addWidget( m_pOrganizationL, 1 , 1         );
   mainLayout->addWidget( m_pCallNumberL  , 2 , 1         );
   mainLayout->addWidget( m_pEmailL       , 3 , 1         );
   mainLayout->addItem(verticalSpacer     , 4 , 1         );

   setLayout(mainLayout);
   setMinimumSize(QSize(50, 30));

   updated();
   connect(this,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(showContext(QPoint)));
}

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
   if (!m_pContactKA->getOrganization().isEmpty()) {
      m_pOrganizationL->setText(m_pContactKA->getOrganization());
   }
   else {
      m_pOrganizationL->setVisible(false);
   }

   if (!getEmail().isEmpty()) {
      m_pEmailL->setText(getEmail());
   }
   else {
      m_pEmailL->setVisible(false);
   }
   
   PhoneNumbers numbers = m_pContactKA->getPhoneNumbers();
   foreach (Contact::PhoneNumber* number, numbers) {
      qDebug() << "Phone:" << number->getNumber() << number->getType();
   }

   if (getCallNumbers().count() == 1)
      m_pCallNumberL->setText(getCallNumbers()[0]->getNumber());
   else
      m_pCallNumberL->setText(QString::number(getCallNumbers().count())+i18n(" numbers"));

   if (!m_pContactKA->getPhoto())
      m_pIconL->setPixmap(QPixmap(KIcon("user-identity").pixmap(QSize(48,48))));
   else
      m_pIconL->setPixmap(*m_pContactKA->getPhoto());
}


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
}

///Send an email
//TODO
void ContactItemWidget::sendEmail()
{
   qDebug() << "Sending email";
}

///Call the same number again
//TODO
void ContactItemWidget::callAgain()
{
   qDebug() << "Calling ";
}

///Copy contact to clipboard
void ContactItemWidget::copy()
{
   qDebug() << "Copying contact";
   QMimeData* mimeData = new QMimeData();
   mimeData->setData(MIME_CONTACT, m_pContactKA->getUid().toUtf8());
   QClipboard* clipboard = QApplication::clipboard();
   clipboard->setMimeData(mimeData);
}

///Edit this contact
void ContactItemWidget::editContact()
{
   qDebug() << "Edit contact";
   AkonadiBackend::getInstance()->editContact(m_pContactKA);
}

///Add a new phone number for this contact
//TODO
void ContactItemWidget::addPhone()
{
   qDebug() << "Adding to contact";
}

///Add this contact to the bookmark list
void ContactItemWidget::bookmark()
{
   PhoneNumbers numbers = m_pContactKA->getPhoneNumbers();
   if (numbers.count() == 1)
      SFLPhone::app()->bookmarkDock()->addBookmark(numbers[0]->getNumber());
}