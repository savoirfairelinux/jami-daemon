/****************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                               *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include "dlgaddressbook.h"

#include "klib/ConfigurationSkeleton.h"

///Constructor
DlgAddressBook::DlgAddressBook(QWidget *parent)
 : QWidget(parent)
{
   setupUi(this);
   
   m_pPhoneTypeList->addItem( m_mNumbertype["Work"]             = new QListWidgetItem("Work"             ));
   m_pPhoneTypeList->addItem( m_mNumbertype["Home"]             = new QListWidgetItem("Home"             ));
   m_pPhoneTypeList->addItem( m_mNumbertype["Messenger"]        = new QListWidgetItem("Messenger"        ));
   m_pPhoneTypeList->addItem( m_mNumbertype["Preferred number"] = new QListWidgetItem("Preferred number" ));
   m_pPhoneTypeList->addItem( m_mNumbertype["Voice"]            = new QListWidgetItem("Voice"            ));
   m_pPhoneTypeList->addItem( m_mNumbertype["Mobile"]           = new QListWidgetItem("Mobile"           ));
   m_pPhoneTypeList->addItem( m_mNumbertype["Video"]            = new QListWidgetItem("Video"            ));
   m_pPhoneTypeList->addItem( m_mNumbertype["Mailbox"]          = new QListWidgetItem("Mailbox"          ));
   m_pPhoneTypeList->addItem( m_mNumbertype["Modem"]            = new QListWidgetItem("Modem"            ));
   m_pPhoneTypeList->addItem( m_mNumbertype["Car"]              = new QListWidgetItem("Car"              ));
   m_pPhoneTypeList->addItem( m_mNumbertype["ISDN"]             = new QListWidgetItem("ISDN"             ));
   m_pPhoneTypeList->addItem( m_mNumbertype["PCS"]              = new QListWidgetItem("PCS"              ));
   m_pPhoneTypeList->addItem( m_mNumbertype["Pager"]            = new QListWidgetItem("Pager"            ));
   m_pPhoneTypeList->addItem( m_mNumbertype["Other..."]         = new QListWidgetItem("Other..."         ));

   QStringList list = ConfigurationSkeleton::phoneTypeList();
   foreach(QListWidgetItem* i,m_mNumbertype) {
      i->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
      i->setCheckState((list.indexOf(m_mNumbertype.key(i)) != -1)?Qt::Checked:Qt::Unchecked);
   }
} //DlgAddressBook

///Destructor
DlgAddressBook::~DlgAddressBook()
{
}

///Reload the widget
void DlgAddressBook::updateWidgets()
{
   
}

///Save the settings
void DlgAddressBook::updateSettings()
{
   QStringList list;
   foreach(QListWidgetItem* i,m_mNumbertype) {
      if (i->checkState() == Qt::Checked)
         list << m_mNumbertype.key(i);
   }
   ConfigurationSkeleton::setPhoneTypeList(list);
}
