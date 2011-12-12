#ifndef ACCOUNT_VIEW_H
#define ACCOUNT_VIEW_H

#include "lib/Item.h"
#include "lib/Account.h"
#include "widgets/AccountItemWidget.h"

//Qt
class QListWidgetItem;

//SFLPhone
class AccountItemWidget;

class AccountView : public Account, public Item<AccountItemWidget> {
   public:
      //Constructor
      AccountView   ();
      void initItem ();

      //Destructor
      ~AccountView(){};

      //Getters
      QListWidgetItem*   getItem           ()      ;
      AccountItemWidget* getItemWidget     ()      ;
      QColor             getStateColor     ()      ;
      QString            getStateColorName ()      ;
      bool               isChecked         () const;

      //Mutators
      static AccountView* buildExistingAccountFromId ( QString _accountId );
      static AccountView* buildNewAccountFromAlias   ( QString alias      );
      virtual void updateState();
      
   private:
      //Attributes
      QListWidgetItem*   item2;
      AccountItemWidget* itemWidget;

      //Private constructor
      void initItemWidget();
};
#endif
