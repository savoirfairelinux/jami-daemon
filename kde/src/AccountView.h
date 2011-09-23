#ifndef ACCOUNT_VIEW_H
#define ACCOUNT_VIEW_H

#include <QtGui/QListWidgetItem>

#include "lib/Item.h"
#include "lib/Account.h"
#include "widgets/AccountItemWidget.h"
#include "lib/sflphone_const.h"

class AccountView : public Account, public Item<AccountItemWidget> {
   public:
      AccountView();
      static AccountView* buildExistingAccountFromId(QString _accountId);
      static AccountView* buildNewAccountFromAlias(QString alias);
      QListWidgetItem* getItem();
      AccountItemWidget* getItemWidget();
      QColor getStateColor();
      QString getStateColorName();
      bool isChecked() const;
      virtual void updateState();
      void initItem();
      
   private:
      void initItemWidget();
      QListWidgetItem* item2;
      AccountItemWidget* itemWidget;
};
#endif
