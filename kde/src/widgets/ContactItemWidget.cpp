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

#include <QtCore/QStringList>
#include <QtCore/QMimeData>
#include <QtGui/QApplication>
#include <QtGui/QClipboard>
#include <QtGui/QGridLayout>
#include <QtGui/QMenu>

#include <klocale.h>
#include <kdebug.h>
#include <unistd.h>
#include <kaction.h>

#include "lib/sflphone_const.h"
#include "ContactItemWidget.h"
#include "AkonadiBackend.h"

ContactItemWidget::ContactItemWidget(QWidget *parent)
   : QWidget(parent), m_pMenu(0),init(false)
{
   setContextMenuPolicy(Qt::CustomContextMenu);
   m_pCallAgain  = new KAction(this);
   m_pCallAgain->setShortcut(Qt::CTRL + Qt::Key_Enter);
   m_pCallAgain->setText("Call Again");
   m_pCallAgain->setIcon(KIcon(ICON_DIALING));
   m_pEditContact = new KAction(this);
   m_pEditContact->setShortcut(Qt::CTRL + Qt::Key_E);
   m_pEditContact->setText("Edit contact");
   m_pEditContact->setIcon(KIcon("contact-new"));
   m_pCopy       = new KAction(this);
   m_pCopy->setShortcut(Qt::CTRL + Qt::Key_C);
   m_pCopy->setText("Copy");
   m_pCopy->setIcon(KIcon("edit-copy"));
   m_pEmail      = new KAction(this);
   m_pEmail->setShortcut(Qt::CTRL + Qt::Key_M);
   m_pEmail->setText("Send Email");
   m_pEmail->setIcon(KIcon("mail-message-new"));
   m_pAddPhone      = new KAction(this);
   m_pAddPhone->setShortcut(Qt::CTRL + Qt::Key_N);
   m_pAddPhone->setText("Add Phone Number");
   m_pAddPhone->setIcon(KIcon("list-resource-add"));

   connect(m_pCallAgain    ,SIGNAL(triggered()),this,SLOT(callAgain()      ));
   connect(m_pEditContact  ,SIGNAL(triggered()),this,SLOT(editContact()    ));
   connect(m_pCopy         ,SIGNAL(triggered()),this,SLOT(copy()           ));
   connect(m_pEmail        ,SIGNAL(triggered()),this,SLOT(sendEmail()      ));
   connect(m_pAddPhone     ,SIGNAL(triggered()),this,SLOT(addPhone()       ));
}

ContactItemWidget::~ContactItemWidget()
{

}

void ContactItemWidget::setContact(Contact* contact)
{
   m_pContactKA = contact;
   m_pIconL = new QLabel(this);
   m_pContactNameL = new QLabel();
   m_pOrganizationL = new QLabel(this);
   m_pEmailL = new QLabel();
   m_pCallNumberL = new QLabel(this);
   
   m_pIconL->setMinimumSize(70,48);
   m_pIconL->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

   QSpacerItem* verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);

   m_pIconL->setMaximumSize(48,9999);
   m_pIconL->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

   m_pIconL->setPixmap(QPixmap(KIcon("user-identity").pixmap(QSize(48,48))));

   QGridLayout* mainLayout = new QGridLayout(this);
   mainLayout->setContentsMargins(0,0,0,0);
   mainLayout->addWidget(m_pIconL,0,0,4,1);
   mainLayout->addWidget(m_pContactNameL,0,1);
   mainLayout->addWidget(m_pOrganizationL,1,1);
   mainLayout->addWidget(m_pCallNumberL,2,1);
   mainLayout->addWidget(m_pEmailL,3,1);
   mainLayout->addItem(verticalSpacer,4,1);

   setLayout(mainLayout);
   setMinimumSize(QSize(50, 30));

   updated();
   connect(this,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(showContext(QPoint)));
}

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
      m_pCallNumberL->setText(QString::number(getCallNumbers().count())+" numbers");

   if (!m_pContactKA->getPhoto())
      m_pIconL->setPixmap(QPixmap(KIcon("user-identity").pixmap(QSize(48,48))));
   else
      m_pIconL->setPixmap(*m_pContactKA->getPhoto());
}

// QPixmap* ContactItemWidget::getIcon()
// {
//    return new QPixmap();
// }

QString ContactItemWidget::getContactName() const
{
   return m_pContactKA->getFormattedName();
}

PhoneNumbers ContactItemWidget::getCallNumbers() const
{
   return m_pContactKA->getPhoneNumbers();
}

QString ContactItemWidget::getOrganization() const
{
   return m_pContactKA->getOrganization();
}

QString ContactItemWidget::getEmail() const
{
   return m_pContactKA->getPreferredEmail();
}

QPixmap* ContactItemWidget::getPicture() const
{
   return (QPixmap*) m_pContactKA->getPhoto();
}

QTreeWidgetItem* ContactItemWidget::getItem()
{
   return m_pItem;
}

void ContactItemWidget::setItem(QTreeWidgetItem* item)
{
   m_pItem = item;
}

Contact* ContactItemWidget::getContact()
{
   return m_pContactKA;
}

void ContactItemWidget::showContext(const QPoint& pos)
{
   if (!m_pMenu) {
      m_pMenu = new QMenu(this);
      m_pMenu->addAction(m_pCallAgain);
      m_pMenu->addAction(m_pEditContact);
      m_pMenu->addAction(m_pAddPhone);
      m_pMenu->addAction(m_pCopy);
      m_pMenu->addAction(m_pEmail);
   }
   m_pMenu->exec(mapToGlobal(pos));
}


void ContactItemWidget::sendEmail()
{
   qDebug() << "Sending email";
}

void ContactItemWidget::callAgain()
{
   qDebug() << "Calling ";
}

void ContactItemWidget::copy()
{
   qDebug() << "Copying contact";
   QMimeData* mimeData = new QMimeData();
   mimeData->setData(MIME_CONTACT, m_pContactKA->getUid().toUtf8());
   QClipboard* clipboard = QApplication::clipboard();
   clipboard->setMimeData(mimeData);
}

void ContactItemWidget::editContact()
{
   qDebug() << "Edit contact";
   AkonadiBackend::getInstance()->editContact(m_pContactKA);
}

void ContactItemWidget::addPhone()
{
   qDebug() << "Adding to contact";
}