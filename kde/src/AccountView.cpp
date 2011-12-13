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
#include <QtCore/QDebug>
#include <QtGui/QListWidgetItem>

//SFLPhone library
#include "lib/sflphone_const.h"
#include "lib/configurationmanager_interface_singleton.h"

///Constructor
AccountView::AccountView() : Account(), item2(0), itemWidget(0)
{

}

///Init
void AccountView::initItem()
{
   if(item2 != NULL)
      delete item2;
   item2 = new QListWidgetItem();
   item2->setSizeHint(QSize(140,25));
   item2->setFlags(Qt::ItemIsSelectable|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled|Qt::ItemIsEnabled);
   initItemWidget();
}

///Init widget
void AccountView::initItemWidget()
{
   if(itemWidget != NULL)
      delete itemWidget;
        
   bool enabled = getAccountDetail(ACCOUNT_ENABLED) == ACCOUNT_ENABLED_TRUE;
   itemWidget = new AccountItemWidget();
   itemWidget->setEnabled(enabled);
   itemWidget->setAccountText(getAccountDetail(ACCOUNT_ALIAS));

   if(isNew() || !enabled)
      itemWidget->setState(AccountItemWidget::Unregistered);
   else if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED || getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_READY)
      itemWidget->setState(AccountItemWidget::Registered);
   else
      itemWidget->setState(AccountItemWidget::NotWorking);
   connect(itemWidget, SIGNAL(checkStateChanged(bool)), this, SLOT(setEnabled(bool)));
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///Get the current item
QListWidgetItem* AccountView::getItem()
{
   return item2;
}

///Get the current widget
AccountItemWidget* AccountView::getItemWidget()
{
   return itemWidget;
}

///Return the state color
QColor AccountView::getStateColor()
{
   if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_UNREGISTERED)
          return Qt::black;
   if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED || getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_READY)
          return Qt::darkGreen;
   return Qt::red;
}

///Get the color name
QString AccountView::getStateColorName()
{
   if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_UNREGISTERED)
          return "black";
   if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED || getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_READY)
          return "darkGreen";
   return "red";
}

///Is this item checked?
bool AccountView::isChecked() const
{
   return itemWidget->getEnabled();
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Build an item from an account id
AccountView* AccountView::buildExistingAccountFromId(QString _accountId)
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
AccountView* AccountView::buildNewAccountFromAlias(QString alias)
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
   qDebug() << "updateState";
   if(! isNew()) {
      Account::updateState();
      
      AccountItemWidget * itemWidget = getItemWidget();
      if(getAccountDetail(ACCOUNT_ENABLED) != ACCOUNT_ENABLED_TRUE ) {
         qDebug() << "itemWidget->setState(AccountItemWidget::Unregistered);";
         itemWidget->setState(AccountItemWidget::Unregistered);
      }
      else if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED || getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_READY) {
         qDebug() << "itemWidget->setState(AccountItemWidget::Registered);";
         itemWidget->setState(AccountItemWidget::Registered);
      }
      else {
         qDebug() << "itemWidget->setState(AccountItemWidget::NotWorking);";
         itemWidget->setState(AccountItemWidget::NotWorking);
      }
   }
}
