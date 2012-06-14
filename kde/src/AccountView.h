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
#ifndef ACCOUNT_VIEW_H
#define ACCOUNT_VIEW_H

#include "lib/Item.h"
#include "lib/Account.h"
#include "widgets/AccountItemWidget.h"

//Qt
class QListWidgetItem;

//SFLPhone
class AccountItemWidget;

///AccountView: List widgets displaying accounts
class AccountView : public Account, public Item<AccountItemWidget> {
   public:
      //Constructor
      AccountView   ();
      ~AccountView  ();
      void initItem ();


      //Getters
      QListWidgetItem*   getItem           ()      ;
      AccountItemWidget* getItemWidget     ()      ;
      QColor             getStateColor     ()      ;
      const QString&     getStateColorName ()      ;
      bool               isChecked         () const;

      //Mutators
      static AccountView* buildExistingAccountFromId (const QString& _accountId );
      static AccountView* buildNewAccountFromAlias   (const QString& alias      );
      virtual void updateState();

   private:
      //Attributes
      QListWidgetItem*   m_pItem;
      AccountItemWidget* m_pWidget;

      //Private constructor
      void initItemWidget();
};
#endif
