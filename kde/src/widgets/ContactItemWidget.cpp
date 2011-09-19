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

#include <klocale.h>
#include <kdebug.h>
#include <unistd.h>

#include "lib/sflphone_const.h"
#include "ContactItemWidget.h"

ContactItemWidget::ContactItemWidget(QWidget *parent)
   : QWidget(parent), init(false)
{

}

ContactItemWidget::~ContactItemWidget()
{

}

void ContactItemWidget::setContact(KABC::Addressee& contact)
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
}

void ContactItemWidget::updated()
{
   m_pContactNameL->setText("<b>"+m_pContactKA.formattedName()+"</b>");
   if (!m_pContactKA.organization().isEmpty()) {
      m_pOrganizationL->setText(m_pContactKA.organization());
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
   
   KABC::PhoneNumber::List numbers = m_pContactKA.phoneNumbers();
   foreach (KABC::PhoneNumber number, numbers) {
      qDebug() << "Phone:" << number.number() << number.typeLabel();
   }

   if (getCallNumbers().count() == 1)
      m_pCallNumberL->setText(getCallNumbers()[0].number());
   else
      m_pCallNumberL->setText(QString::number(getCallNumbers().count())+" numbers");
   
   QImage photo = m_pContactKA.photo().data();
   if (photo.isNull())
      m_pIconL->setPixmap(QPixmap(KIcon("user-identity").pixmap(QSize(48,48))));
   else
      m_pIconL->setPixmap(QPixmap::fromImage( m_pContactKA.photo().data()).scaled(QSize(48,48)));;
}

QPixmap* ContactItemWidget::getIcon()
{
   return new QPixmap();
}

QString ContactItemWidget::getContactName()
{
   return m_pContactKA.formattedName();
}

KABC::PhoneNumber::List ContactItemWidget::getCallNumbers()
{
   return m_pContactKA.phoneNumbers();
}

QString ContactItemWidget::getOrganization()
{
   return m_pContactKA.organization();
}

QString ContactItemWidget::getEmail()
{
   return m_pContactKA.fullEmail();
}

QString ContactItemWidget::getPicture()
{
   return m_pContactKA.photo().url();
}

QTreeWidgetItem* ContactItemWidget::getItem()
{
   return m_pItem;
}

void ContactItemWidget::setItem(QTreeWidgetItem* item)
{
   m_pItem = item;
}