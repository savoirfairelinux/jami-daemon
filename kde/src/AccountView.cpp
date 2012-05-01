/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
 *   Author : Emmanuel Lepage Valle <emmanuel.lepage@savoirfairelinux.com >*
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
#include "AccountView.h"


//Qt
#include <QtGui/QListWidgetItem>

//KDE
#include <KDebug>

//SFLPhone library
#include "lib/sflphone_const.h"
#include "lib/configurationmanager_interface_singleton.h"

///Constructor
AccountView::AccountView() : Account(), m_pItem(0), m_pWidget(0)
{

}

///Init
void AccountView::initItem()
{
   if(m_pItem != NULL)
      delete m_pItem;
   m_pItem = new QListWidgetItem();
   m_pItem->setSizeHint(QSize(140,25));
   m_pItem->setFlags(Qt::ItemIsSelectable|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled|Qt::ItemIsEnabled);
   initItemWidget();
}

///Init widget
void AccountView::initItemWidget()
{
   if(m_pWidget != NULL)
      delete m_pWidget;

   bool enabled = getAccountDetail(ACCOUNT_ENABLED) == REGISTRATION_ENABLED_TRUE;
   m_pWidget = new AccountItemWidget();
   m_pWidget->setEnabled(enabled);
   m_pWidget->setAccountText(getAccountDetail(ACCOUNT_ALIAS));

   if(isNew() || !enabled)
      m_pWidget->setState(AccountItemWidget::Unregistered);
   else if(getAccountDetail(REGISTRATION_STATUS) == ACCOUNT_STATE_REGISTERED || getAccountDetail(REGISTRATION_STATUS) == ACCOUNT_STATE_READY)
      m_pWidget->setState(AccountItemWidget::Registered);
   else
      m_pWidget->setState(AccountItemWidget::NotWorking);
   connect(m_pWidget, SIGNAL(checkStateChanged(bool)), this, SLOT(setEnabled(bool)));
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///Get the current item
QListWidgetItem* AccountView::getItem()
{
   return m_pItem;
}

///Get the current widget
AccountItemWidget* AccountView::getItemWidget()
{
   return m_pWidget;
}

///Return the state color
QColor AccountView::getStateColor()
{
   if(getAccountDetail(REGISTRATION_STATUS) == ACCOUNT_STATE_UNREGISTERED)
          return Qt::black;
   if(getAccountDetail(REGISTRATION_STATUS) == ACCOUNT_STATE_REGISTERED || getAccountDetail(REGISTRATION_STATUS) == ACCOUNT_STATE_READY)
          return Qt::darkGreen;
   return Qt::red;
}

///Get the color name
const QString& AccountView::getStateColorName()
{
   static const QString black    ( "black"     );
   static const QString darkGreen( "darkGreen" );
   static const QString red      ( "red"       );
   if(getAccountDetail(REGISTRATION_STATUS) == ACCOUNT_STATE_UNREGISTERED)
          return black;
   if(getAccountDetail(REGISTRATION_STATUS) == ACCOUNT_STATE_REGISTERED || getAccountDetail(REGISTRATION_STATUS) == ACCOUNT_STATE_READY)
          return darkGreen;
   return red;
}

///Is this item checked?
bool AccountView::isChecked() const
{
   return m_pWidget->getEnabled();
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Build an item from an account id
AccountView* AccountView::buildExistingAccountFromId(const QString& _accountId)
{
   //Account* a = Account::buildExistingAccountFromId( _accountId);
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   AccountView* a = new AccountView();
   a->m_pAccountId = new QString(_accountId);
   a->m_pAccountDetails = new MapStringString( configurationManager.getAccountDetails(_accountId).value() );
   a->initItem();
   return a;
}

///Build an item from an alias
AccountView* AccountView::buildNewAccountFromAlias(const QString& alias)
{
   //Account* a = Account::buildNewAccountFromAlias(alias);
   AccountView* a = new AccountView();
   a->m_pAccountDetails = new MapStringString();
   a->setAccountDetail(ACCOUNT_ALIAS,alias);
   a->initItem();
   return a;
}

///Change LED color
void AccountView::updateState()
{
   if(! isNew()) {
      Account::updateState();

      AccountItemWidget * m_pWidget = getItemWidget();
      if(getAccountDetail(ACCOUNT_ENABLED) != REGISTRATION_ENABLED_TRUE ) {
         kDebug() << "Changing account state to Unregistered";
         m_pWidget->setState(AccountItemWidget::Unregistered);
      }
      else if(getAccountDetail(REGISTRATION_STATUS) == ACCOUNT_STATE_REGISTERED || getAccountDetail(REGISTRATION_STATUS) == ACCOUNT_STATE_READY) {
         kDebug() << "Changing account state to  Registered";
         m_pWidget->setState(AccountItemWidget::Registered);
      }
      else {
         kDebug() << "Changing account state to NotWorking";
         m_pWidget->setState(AccountItemWidget::NotWorking);
      }
   }
}
