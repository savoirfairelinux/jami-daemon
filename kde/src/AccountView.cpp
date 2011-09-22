#include "AccountView.h"

#include <QDebug>

#include "lib/sflphone_const.h"
#include "lib/configurationmanager_interface_singleton.h"

AccountView::AccountView() : Account(), item2(0), itemWidget(0)
{

}

void AccountView::initItem()
{
   if(item2 != NULL)
      delete item2;
   item2 = new QListWidgetItem();
   item2->setSizeHint(QSize(140,25));
   item2->setFlags(Qt::ItemIsSelectable|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled|Qt::ItemIsEnabled);
   initItemWidget();
}

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

QListWidgetItem* AccountView::getItem()
{
   return item2;
}

AccountItemWidget* AccountView::getItemWidget()
{
   return itemWidget;
}

QColor AccountView::getStateColor()
{
   if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_UNREGISTERED)
          return Qt::black;
   if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED || getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_READY)
          return Qt::darkGreen;
   return Qt::red;
}


QString AccountView::getStateColorName()
{
   if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_UNREGISTERED)
          return "black";
   if(getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED || getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_READY)
          return "darkGreen";
   return "red";
}

bool AccountView::isChecked() const
{
   return itemWidget->getEnabled();
}

AccountView* AccountView::buildExistingAccountFromId(QString _accountId)
{
   //Account* a = Account::buildExistingAccountFromId( _accountId);
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   AccountView* a = new AccountView();
   a->accountId = new QString(_accountId);
   a->accountDetails = new MapStringString( configurationManager.getAccountDetails(_accountId).value() );
   a->initItem();
   return a;
}

AccountView* AccountView::buildNewAccountFromAlias(QString alias)
{
   //Account* a = Account::buildNewAccountFromAlias(alias);
   AccountView* a = new AccountView();
   a->accountDetails = new MapStringString();
   a->setAccountDetail(ACCOUNT_ALIAS,alias);
   a->initItem();
   return a;
}

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
