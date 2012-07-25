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
#include "dlggeneral.h"

//Qt
#include <QtGui/QToolButton>
#include <QtGui/QAction>

//SFLPhone
#include "klib/ConfigurationSkeleton.h"
#include "conf/ConfigurationDialog.h"
#include "lib/configurationmanager_interface_singleton.h"

///Constructor
DlgGeneral::DlgGeneral(KConfigDialog *parent)
 : QWidget(parent),m_HasChanged(false)
{
   setupUi(this);
   connect(toolButton_historyClear, SIGNAL(clicked()), this, SIGNAL(clearCallHistoryAsked()));

   kcfg_historyMax->setValue(ConfigurationSkeleton::historyMax());
   kcfg_minimumRowHeight->setEnabled(ConfigurationSkeleton::limitMinimumRowHeight());

   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   m_pAlwaysRecordCK->setChecked(configurationManager.getIsAlwaysRecording());

   //Need to be ordered
   m_lCallDetails[ "Display Icon"         ] = "displayCallIcon"        ;
   m_lCallDetails[ "Display Security"     ] = "displayCallSecure"      ;
   m_lCallDetails[ "Display Codec"        ] = "displayCallCodec"       ;
   m_lCallDetails[ "Display Call Number"  ] = "displayCallNumber"      ;
   m_lCallDetails[ "Display Peer Name"    ] = "displayCallPeer"        ;
   m_lCallDetails[ "Display organisation" ] = "displayCallOrganisation";
   m_lCallDetails[ "Display department"   ] = "displayCallDepartment"  ;
   m_lCallDetails[ "Display e-mail"       ] = "displayCallEmail"       ;

   QMutableMapIterator<QString, QString> iter(m_lCallDetails);
   while (iter.hasNext()) {
      iter.next();
      bool checked = ConfigurationSkeleton::self()->findItem(iter.value())->isEqual(true);
      QListWidgetItem* i = new QListWidgetItem(i18n(iter.key().toAscii()));
      i->setFlags      (Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
      i->setCheckState ((checked)?Qt::Checked:Qt::Unchecked        );
      m_pDetailsList->addItem(m_lItemList[iter.value()] = i);
   }
   connect(m_pDetailsList   , SIGNAL(itemChanged(QListWidgetItem*))  , this  , SLOT(changed())      );
   connect(m_pAlwaysRecordCK, SIGNAL(clicked(bool))                  , this  , SLOT(changed())      );
   connect(this             , SIGNAL(updateButtons())                , parent, SLOT(updateButtons()));
}

///Destructor
DlgGeneral::~DlgGeneral()
{
}

///Have this dialog changed
bool DlgGeneral::hasChanged()
{
   return m_HasChanged;
}

///Tag this dialog as changed
void DlgGeneral::changed()
{
   m_HasChanged = true;
   emit updateButtons();
}

///Update all widgets
void DlgGeneral::updateWidgets()
{
}

///Save current settings
void DlgGeneral::updateSettings()
{
   QMutableMapIterator<QString, QString> iter(m_lCallDetails);
   while (iter.hasNext()) {
      iter.next();
      ConfigurationSkeleton::self()->findItem(iter.value())->setProperty(m_lItemList[iter.value()]->checkState() == Qt::Checked);
   }
   ConfigurationSkeleton::setHistoryMax(kcfg_historyMax->value());

   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   configurationManager.setIsAlwaysRecording(m_pAlwaysRecordCK->isChecked());

   m_HasChanged = false;
}
