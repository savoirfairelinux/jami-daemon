/***************************************************************************
 *   Copyright (C) 2009-2010 by Savoir-Faire Linux                         *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
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
#include "dlgaccounts.h"

#include <QtGui/QInputDialog>

#include "lib/configurationmanager_interface_singleton.h"
#include "SFLPhoneView.h"
#include "lib/sflphone_const.h"
#include "conf/ConfigurationDialog.h"
#include <vector>
#include <string>

DlgAccounts::DlgAccounts(KConfigDialog* parent)
 : QWidget(parent)
{
   setupUi(this);
   
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   button_accountUp->setIcon     (KIcon("go-up")       );
   button_accountDown->setIcon   (KIcon("go-down")     );
   button_accountAdd->setIcon    (KIcon("list-add")    );
   button_accountRemove->setIcon (KIcon("list-remove") );
   accountList = new ConfigAccountList(false);
   loadAccountList();
   loadCodecList();
   accountListHasChanged = false;
   //toolButton_accountsApply->setEnabled(false);

   //SLOTS
   //                     SENDER                            SIGNAL                  RECEIVER            SLOT                   /
   /**/connect(edit1_alias,                    SIGNAL(textEdited(const QString &)) , this   , SLOT(changedAccountList()      ));
   /**/connect(edit2_protocol,                 SIGNAL(activated(int))              , this   , SLOT(changedAccountList()      ));
   /**/connect(edit3_server,                   SIGNAL(textEdited(const QString &)) , this   , SLOT(changedAccountList()      ));
   /**/connect(edit4_user,                     SIGNAL(textEdited(const QString &)) , this   , SLOT(changedAccountList()      ));
   /**/connect(edit5_password,                 SIGNAL(textEdited(const QString &)) , this   , SLOT(changedAccountList()      ));
   /**/connect(edit6_mailbox,                  SIGNAL(textEdited(const QString &)) , this   , SLOT(changedAccountList()      ));
   /**/connect(spinbox_regExpire,              SIGNAL(editingFinished())           , this   , SLOT(changedAccountList()      ));
   /**/connect(comboBox_ni_local_address,      SIGNAL(currentIndexChanged (int))   , this   , SLOT(changedAccountList()      ));
   /**/connect(checkBox_conformRFC,            SIGNAL(clicked(bool))               , this   , SLOT(changedAccountList()      ));
   /**/connect(button_accountUp,               SIGNAL(clicked())                   , this   , SLOT(changedAccountList()      ));
   /**/connect(button_accountDown,             SIGNAL(clicked())                   , this   , SLOT(changedAccountList()      ));
   /**/connect(button_accountAdd,              SIGNAL(clicked())                   , this   , SLOT(changedAccountList()      ));
   /**/connect(button_accountRemove,           SIGNAL(clicked())                   , this   , SLOT(changedAccountList()      ));
   /**/connect(edit_tls_private_key_password,  SIGNAL(textEdited(const QString &)) , this   , SLOT(changedAccountList()      ));
   /**/connect(spinbox_tls_listener,           SIGNAL(editingFinished())           , this   , SLOT(changedAccountList()      ));
   /**/connect(file_tls_authority,             SIGNAL(textChanged(const QString &)), this   , SLOT(changedAccountList()      ));
   /**/connect(file_tls_endpoint,              SIGNAL(textChanged(const QString &)), this   , SLOT(changedAccountList()      ));
   /**/connect(file_tls_private_key,           SIGNAL(textChanged(const QString &)), this   , SLOT(changedAccountList()      ));
   /**/connect(combo_tls_method,               SIGNAL(currentIndexChanged(int))    , this   , SLOT(changedAccountList()      ));
   /**/connect(edit_tls_cipher,                SIGNAL(textEdited(const QString &)) , this   , SLOT(changedAccountList()      ));
   /**/connect(edit_tls_outgoing,              SIGNAL(textEdited(const QString &)) , this   , SLOT(changedAccountList()      ));
   /**/connect(spinbox_tls_timeout_sec,        SIGNAL(editingFinished())           , this   , SLOT(changedAccountList()      ));
   /**/connect(spinbox_tls_timeout_msec,       SIGNAL(editingFinished())           , this   , SLOT(changedAccountList()      ));
   /**/connect(check_tls_incoming,             SIGNAL(clicked(bool))               , this   , SLOT(changedAccountList()      ));
   /**/connect(check_tls_answer,               SIGNAL(clicked(bool))               , this   , SLOT(changedAccountList()      ));
   /**/connect(check_tls_requier_cert,         SIGNAL(clicked(bool))               , this   , SLOT(changedAccountList()      ));
   /**/connect(group_security_tls,             SIGNAL(clicked(bool))               , this   , SLOT(changedAccountList()      ));
   /**/connect(radioButton_pa_same_as_local,   SIGNAL(clicked(bool))               , this   , SLOT(changedAccountList()      ));
   /**/connect(radioButton_pa_custom,          SIGNAL(clicked(bool))               , this   , SLOT(changedAccountList()      ));
   /**/connect(&configurationManager,          SIGNAL(accountsChanged())           , this   , SLOT(updateAccountStates()     ));
   /**/connect(edit_tls_private_key_password,  SIGNAL(textEdited(const QString &)) , this   , SLOT(changedAccountList()      ));
   /**/connect(this,                           SIGNAL(updateButtons())             , parent , SLOT(updateButtons()           ));
   /**/connect(keditlistbox_codec->listView(), SIGNAL(clicked(QModelIndex))        , this   , SLOT(codecClicked(QModelIndex) ));
   /**/connect(keditlistbox_codec->addButton(),SIGNAL(clicked())                   , this   , SLOT(addCodec()                ));
   /**/connect(keditlistbox_codec,             SIGNAL(changed())                   , this   , SLOT(codecChanged()            ));
   /**/connect(combo_security_STRP,            SIGNAL(currentIndexChanged(int))    , this   , SLOT(updateCombo(int)          ));
   /**/connect(button_add_credential,          SIGNAL(clicked())                   , this   , SLOT(addCredential()           ));
   /**/connect(button_remove_credential,       SIGNAL(clicked())                   , this   , SLOT(removeCredential()        ));
   /*                                                                                                                         */


   connect(list_credential,                SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this,SLOT( selectCredential(QListWidgetItem*, QListWidgetItem*)));
   
   //Disable control
   connect(radioButton_pa_same_as_local,   SIGNAL(clicked(bool))               , this   , SLOT(enablePublished()));
   connect(radioButton_pa_custom,          SIGNAL(clicked(bool))               , this   , SLOT(enablePublished()));
}

void DlgAccounts::saveAccountList()
{
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   disconnectAccountsChangedSignal();

   //save the account being edited
   if(listWidget_accountList->currentItem()) {
      saveAccount(listWidget_accountList->currentItem());
   }
   QStringList accountIds= QStringList(configurationManager.getAccountList().value());

   //create or update each account from accountList
   for (int i = 0; i < accountList->size(); i++) {
      AccountView* current = (*accountList)[i];
      QString currentId;
      //if the account has no instanciated id, it has just been created in the client
      if(current->isNew()) {
         MapStringString details = current->getAccountDetails();
         currentId = configurationManager.addAccount(details);
         current->setAccountId(currentId);
      }
      //if the account has an instanciated id but it's not in configurationManager
      else {
         if(! accountIds.contains(current->getAccountId())) {
            qDebug() << "The account with id " << current->getAccountId() << " doesn't exist. It might have been removed by another SFLphone client.";
            currentId = QString();
         }
         else {
            configurationManager.setAccountDetails(current->getAccountId(), current->getAccountDetails());
            currentId = QString(current->getAccountId());
         }
      }
   }
   //remove accounts that are in the configurationManager but not in the client
   for (int i = 0; i < accountIds.size(); i++) {
      if(! accountList->getAccountById(accountIds[i])) {
         qDebug() << "remove account " << accountIds[i];
         configurationManager.removeAccount(accountIds[i]);
      }
   }
   configurationManager.setAccountsOrder(accountList->getOrderedList());
   connectAccountsChangedSignal();
}

void DlgAccounts::connectAccountsChangedSignal()
{
   qDebug() << "connectAccountsChangedSignal";
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   connect(&configurationManager, SIGNAL(accountsChanged()),
           this,                  SLOT(updateAccountStates()));
}

void DlgAccounts::disconnectAccountsChangedSignal()
{
   qDebug() << "disconnectAccountsChangedSignal";
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   disconnect(&configurationManager, SIGNAL(accountsChanged()),
           this,                  SLOT(updateAccountStates()));
}


void DlgAccounts::saveAccount(QListWidgetItem * item)
{
   QString protocolsTab[] = ACCOUNT_TYPES_TAB;

   if(! item) { 
      qDebug() << "Attempting to save details of an account from a NULL item"; 
       return; 
   }
   
   AccountView* account = accountList->getAccountByItem(item);
   if(!account) {
      qDebug() << "Attempting to save details of an unexisting account : " << item->text();  
      return;  
   }
   //ACCOUNT DETAILS
   //                                     FIELD                                             WIDGET VALUE                                     /
   /**/account->setAccountDetail( ACCOUNT_ALIAS                  , edit1_alias->text()                                                      );
   /**/account->setAccountDetail( ACCOUNT_TYPE                   , protocolsTab[edit2_protocol->currentIndex()]                             );
   /**/account->setAccountDetail( ACCOUNT_HOSTNAME               , edit3_server->text()                                                     );
   /**/account->setAccountDetail( ACCOUNT_USERNAME               , edit4_user->text()                                                       );
   /**/account->setAccountDetail( ACCOUNT_PASSWORD               , edit5_password->text()                                                   );
   /**/account->setAccountDetail( ACCOUNT_MAILBOX                , edit6_mailbox->text()                                                    );
   /**/account->setAccountDetail( ACCOUNT_ENABLED                , account->isChecked() ? ACCOUNT_ENABLED_TRUE : ACCOUNT_ENABLED_FALSE      );
   /**/                                                                                                                                   /**/
   /*                                                               Security                                                                */
   /**/account->setAccountDetail( TLS_PASSWORD                   , edit_tls_private_key_password->text()                                    );
   /**/account->setAccountDetail( TLS_LISTENER_PORT              , QString::number(spinbox_tls_listener->value())                           );
   /**/account->setAccountDetail( TLS_CA_LIST_FILE               , file_tls_authority->text()                                               );
   /**/account->setAccountDetail( TLS_CERTIFICATE_FILE           , file_tls_endpoint->text()                                                );
   /**/account->setAccountDetail( TLS_PRIVATE_KEY_FILE           , file_tls_private_key->text()                                             );
   /**/account->setAccountDetail( TLS_METHOD                     , combo_tls_method->currentText()                                          );
   /**/account->setAccountDetail( TLS_CIPHERS                    , edit_tls_cipher->text()                                                  );
   /**/account->setAccountDetail( TLS_SERVER_NAME                , edit_tls_outgoing->text()                                                );
   /**/account->setAccountDetail( TLS_NEGOTIATION_TIMEOUT_SEC    , QString::number(spinbox_tls_timeout_sec->value())                        );
   /**/account->setAccountDetail( TLS_NEGOTIATION_TIMEOUT_MSEC   , QString::number(spinbox_tls_timeout_msec->value())                       );
   /**/account->setAccountDetail( TLS_METHOD                     , QString::number(combo_security_STRP->currentIndex())                     );
   /**/account->setAccountDetail( TLS_VERIFY_SERVER              , check_tls_incoming->isChecked()                          ?"true":"false" );
   /**/account->setAccountDetail( TLS_VERIFY_CLIENT              , check_tls_answer->isChecked()                            ?"true":"false" );
   /**/account->setAccountDetail( TLS_REQUIRE_CLIENT_CERTIFICATE , check_tls_requier_cert->isChecked()                      ?"true":"false" );
   /**/account->setAccountDetail( TLS_ENABLE                     , group_security_tls->isChecked()                          ?"true":"false" );
   /**/account->setAccountDetail( ACCOUNT_DISPLAY_SAS_ONCE       , checkbox_ZRTP_Ask_user->isChecked()                      ?"true":"false" );
   /**/account->setAccountDetail( ACCOUNT_SRTP_RTP_FALLBACK      , checkbox_SDES_fallback_rtp->isChecked()                  ?"true":"false" );
   /**/account->setAccountDetail( ACCOUNT_ZRTP_DISPLAY_SAS       , checkbox_ZRTP_display_SAS->isChecked()                   ?"true":"false" );
   /**/account->setAccountDetail( ACCOUNT_ZRTP_NOT_SUPP_WARNING  , checkbox_ZRTP_warn_supported->isChecked()                ?"true":"false" );
   /**/account->setAccountDetail( ACCOUNT_ZRTP_HELLO_HASH        , checkbox_ZTRP_send_hello->isChecked()                    ?"true":"false" );
   /**/account->setAccountDetail( ACCOUNT_SIP_STUN_ENABLED       , checkbox_stun->isChecked()                               ?"true":"false" );
   /**/account->setAccountDetail( PUBLISHED_SAMEAS_LOCAL         , radioButton_pa_same_as_local->isChecked()                ?"true":"false" );
   /**/account->setAccountDetail( ACCOUNT_SIP_STUN_SERVER        , line_stun->text()                                                        );
   /**/account->setAccountDetail( PUBLISHED_PORT                 , QString::number(spinBox_pa_published_port->value())                      );
   /**/account->setAccountDetail( PUBLISHED_ADDRESS              , lineEdit_pa_published_address ->text()                                   );
   /**/account->setAccountDetail( LOCAL_PORT                     , QString::number(spinBox_pa_published_port->value())                      );
   /**/account->setAccountDetail( LOCAL_INTERFACE                , comboBox_ni_local_address->currentText()                                 );
   //                                                                                                                                        /
   
   QStringList _codecList;
   foreach (QString aCodec, keditlistbox_codec->items()) {
      foreach (StringHash _aCodec, codecList) {
         if (_aCodec["alias"] == aCodec) {
            _codecList << _aCodec["id"];
         }
      }
   }

   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   configurationManager.setActiveAudioCodecList(_codecList, account->getAccountDetail(ACCOUNT_ID));
   qDebug() << "Account codec have been saved" << _codecList << account->getAccountDetail(ACCOUNT_ID);
   
   saveCredential(account->getAccountDetail(ACCOUNT_ID));
}

void DlgAccounts::loadAccount(QListWidgetItem * item)
{
   if(! item ) { 
      qDebug() << "Attempting to load details of an account from a NULL item";  
      return;  
   }

   AccountView* account = accountList->getAccountByItem(item);
   if(! account ) {  
      qDebug() << "Attempting to load details of an unexisting account";  
      return;  
   }

   edit1_alias->setText( account->getAccountDetail(ACCOUNT_ALIAS));
   
   QString protocolsTab[] = ACCOUNT_TYPES_TAB;
   QList<QString> * protocolsList = new QList<QString>();
   for(int i = 0 ; i < (int) (sizeof(protocolsTab) / sizeof(QString)) ; i++) { 
      protocolsList->append(protocolsTab[i]);
   }
   
   QString accountName = account->getAccountDetail(ACCOUNT_TYPE);
   int protocolIndex = protocolsList->indexOf(accountName);
   delete protocolsList;



   loadCredentails(account->getAccountDetail(ACCOUNT_ID));
   
   bool ok;
   int val = account->getAccountDetail(ACCOUNT_EXPIRE).toInt(&ok);
   spinbox_regExpire->setValue(ok ? val : ACCOUNT_EXPIRE_DEFAULT);

   if (credentialList.size())
      edit5_password->setText( credentialList[0].password );


   
   
   switch (account->getAccountDetail(TLS_METHOD ).toInt()) {
      case 0: //KEY_EXCHANGE_NONE
         checkbox_SDES_fallback_rtp->setVisible   ( false );
         checkbox_ZRTP_Ask_user->setVisible       ( false );
         checkbox_ZRTP_display_SAS->setVisible    ( false );
         checkbox_ZRTP_warn_supported->setVisible ( false );
         checkbox_ZTRP_send_hello->setVisible     ( false );
         break;
      case 1: //ZRTP
         checkbox_SDES_fallback_rtp->setVisible   ( false );
         checkbox_ZRTP_Ask_user->setVisible       ( true  );
         checkbox_ZRTP_display_SAS->setVisible    ( true  );
         checkbox_ZRTP_warn_supported->setVisible ( true  );
         checkbox_ZTRP_send_hello->setVisible     ( true  );
         break;
      case 2: //SDES
         checkbox_SDES_fallback_rtp->setVisible   ( true  );
         checkbox_ZRTP_Ask_user->setVisible       ( false );
         checkbox_ZRTP_display_SAS->setVisible    ( false );
         checkbox_ZRTP_warn_supported->setVisible ( false );
         checkbox_ZTRP_send_hello->setVisible     ( false );
         break;
   }
   //         WIDGET VALUE                                                          FIELD                          VALUE        /
   /**/edit2_protocol->setCurrentIndex          ( (protocolIndex < 0) ? 0 : protocolIndex                                      );
   /**/edit3_server->setText                    ( account->getAccountDetail(   ACCOUNT_HOSTNAME              )                 );
   /**/edit4_user->setText                      ( account->getAccountDetail(   ACCOUNT_USERNAME              )                 );
   /**/edit6_mailbox->setText                   ( account->getAccountDetail(   ACCOUNT_MAILBOX               )                 );
   /**/checkBox_conformRFC->setChecked          ( account->getAccountDetail(   ACCOUNT_RESOLVE_ONCE          )  != "TRUE"      );
   /**/checkbox_ZRTP_Ask_user->setChecked       ( (account->getAccountDetail(  ACCOUNT_DISPLAY_SAS_ONCE      )  == "true")?1:0 );
   /**/checkbox_SDES_fallback_rtp->setChecked   ( (account->getAccountDetail(  ACCOUNT_SRTP_RTP_FALLBACK     )  == "true")?1:0 );
   /**/checkbox_ZRTP_display_SAS->setChecked    ( (account->getAccountDetail(  ACCOUNT_ZRTP_DISPLAY_SAS      )  == "true")?1:0 );
   /**/checkbox_ZRTP_warn_supported->setChecked ( (account->getAccountDetail(  ACCOUNT_ZRTP_NOT_SUPP_WARNING )  == "true")?1:0 );
   /**/checkbox_ZTRP_send_hello->setChecked     ( (account->getAccountDetail(  ACCOUNT_ZRTP_HELLO_HASH       )  == "true")?1:0 );
   /**/checkbox_stun->setChecked                ( (account->getAccountDetail(  ACCOUNT_SIP_STUN_ENABLED      )  == "true")?1:0 );
   /**/line_stun->setText                       ( account->getAccountDetail(   ACCOUNT_SIP_STUN_SERVER       )                 );
   /**/radioButton_pa_same_as_local->setChecked ( (account->getAccountDetail(  PUBLISHED_SAMEAS_LOCAL        )  == "true")?1:0 );
   /**/radioButton_pa_custom->setChecked        ( !(account->getAccountDetail( PUBLISHED_SAMEAS_LOCAL        )  == "true")?1:0 );
   /**/lineEdit_pa_published_address->setText   ( account->getAccountDetail(   PUBLISHED_ADDRESS             )                 );
   /**/spinBox_pa_published_port->setValue      ( account->getAccountDetail(   PUBLISHED_PORT).toUInt()                        );
   /*                                                   Security                                                               */
   /**/edit_tls_private_key_password->setText   ( account->getAccountDetail(   TLS_PASSWORD                  )                 );
   /**/spinbox_tls_listener->setValue           ( account->getAccountDetail(   TLS_LISTENER_PORT             ).toInt()         );
   /**/file_tls_authority->setText              ( account->getAccountDetail(   TLS_CA_LIST_FILE              )                 );
   /**/file_tls_endpoint->setText               ( account->getAccountDetail(   TLS_CERTIFICATE_FILE          )                 );
   /**/file_tls_private_key->setText            ( account->getAccountDetail(   TLS_PRIVATE_KEY_FILE          )                 );
   /**/edit_tls_cipher->setText                 ( account->getAccountDetail(   TLS_CIPHERS                   )                 );
   /**/edit_tls_outgoing->setText               ( account->getAccountDetail(   TLS_SERVER_NAME               )                 );
   /**/spinbox_tls_timeout_sec->setValue        ( account->getAccountDetail(   TLS_NEGOTIATION_TIMEOUT_SEC ).toInt()           );
   /**/spinbox_tls_timeout_msec->setValue       ( account->getAccountDetail(   TLS_NEGOTIATION_TIMEOUT_MSEC ).toInt()          );
   /**/check_tls_incoming->setChecked           ( (account->getAccountDetail(  TLS_VERIFY_SERVER             )  == "true")?1:0 );
   /**/check_tls_answer->setChecked             ( (account->getAccountDetail(  TLS_VERIFY_CLIENT             )  == "true")?1:0 );
   /**/check_tls_requier_cert->setChecked       ( (account->getAccountDetail(  TLS_REQUIRE_CLIENT_CERTIFICATE)  == "true")?1:0 );
   /**/group_security_tls->setChecked           ( (account->getAccountDetail(  TLS_ENABLE                    )  == "true")?1:0 );
   /**/combo_security_STRP->setCurrentIndex     ( account->getAccountDetail(   TLS_METHOD                    ).toInt()         );
   /*                                                                                                                          */
   
   combo_tls_method->setCurrentIndex        ( combo_tls_method->findText(account->getAccountDetail(TLS_METHOD )));
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();

   comboBox_ni_local_address->clear();
   QStringList interfaceList = configurationManager.getAllIpInterfaceByName();
   comboBox_ni_local_address->addItems(interfaceList);
   
   spinBox_ni_local_port->setValue(account->getAccountDetail(LOCAL_PORT).toInt());
   comboBox_ni_local_address->setCurrentIndex(comboBox_ni_local_address->findText(account->getAccountDetail(LOCAL_INTERFACE))); //TODO need to load the list first

   QStringList activeCodecList = configurationManager.getActiveAudioCodecList(account->getAccountDetail(ACCOUNT_ID));
   keditlistbox_codec->clear();
   foreach (QString aCodec, activeCodecList) {
      foreach (StringHash _aCodec, codecList) {
;         if (_aCodec["id"] == aCodec)
            keditlistbox_codec->insertItem(_aCodec["alias"]);
      }
   }
        
        

   if(protocolIndex == 0) { // if sip selected
      checkbox_stun->setChecked(account->getAccountDetail(ACCOUNT_SIP_STUN_ENABLED) == ACCOUNT_ENABLED_TRUE);
      line_stun->setText( account->getAccountDetail(ACCOUNT_SIP_STUN_SERVER) );
      //checkbox_zrtp->setChecked(account->getAccountDetail(ACCOUNT_SRTP_ENABLED) == ACCOUNT_ENABLED_TRUE);

      tab_advanced->setEnabled(true);
      line_stun->setEnabled(checkbox_stun->isChecked());
   }
   else {
      checkbox_stun->setChecked(false);
      line_stun->setText( account->getAccountDetail(ACCOUNT_SIP_STUN_SERVER) );
      //checkbox_zrtp->setChecked(false);

      tab_advanced->setEnabled(false);
   }

   updateStatusLabel(account);
   frame2_editAccounts->setEnabled(true);
}

void DlgAccounts::loadAccountList()
{
   qDebug() << "loadAccountList";
   accountList->updateAccounts();
   listWidget_accountList->clear();
   for (int i = 0; i < accountList->size(); ++i) {
      addAccountToAccountList((*accountList)[i]);
   }
   if (listWidget_accountList->count() > 0 && listWidget_accountList->currentItem() == NULL) 
      listWidget_accountList->setCurrentRow(0);
   else 
      frame2_editAccounts->setEnabled(false);
}

void DlgAccounts::addAccountToAccountList(AccountView* account)
{
   QListWidgetItem * item = account->getItem();
   QWidget * widget = account->getItemWidget();
   connect(widget, SIGNAL(checkStateChanged(bool)),
           this,   SLOT(changedAccountList()));
   listWidget_accountList->addItem(item);
   listWidget_accountList->setItemWidget(item, widget);
}

void DlgAccounts::changedAccountList()
{
   qDebug() << "changedAccountList";
   accountListHasChanged = true;
   emit updateButtons();
   //toolButton_accountsApply->setEnabled(true);
//<<<<<<< HEAD
   
//   int currentIndex = edit2_protocol->currentIndex();

//   if(currentIndex==0)
//   {
//      tab_advanced->setEnabled(true);
//      line_stun->setEnabled(checkbox_stun->isChecked());
//   }
//   else
//   {
//      tab_advanced->setEnabled(false);
//   }
//=======
//>>>>>>> master
}



void DlgAccounts::on_listWidget_accountList_currentItemChanged ( QListWidgetItem * current, QListWidgetItem * previous )
{
   qDebug() << "on_listWidget_accountList_currentItemChanged";
   saveAccount(previous);
   loadAccount(current);
   updateAccountListCommands();
}

void DlgAccounts::on_button_accountUp_clicked()
{
   qDebug() << "on_button_accountUp_clicked";
   int currentRow = listWidget_accountList->currentRow();
   QListWidgetItem * prevItem = listWidget_accountList->takeItem(currentRow);
   AccountView* account = accountList->getAccountByItem(prevItem);
   //we need to build a new item to set the itemWidget back
   account->initItem();
   QListWidgetItem * item = account->getItem();
   AccountItemWidget * widget = account->getItemWidget();
   accountList->upAccount(currentRow);
   listWidget_accountList->insertItem     ( currentRow - 1 , item );
   listWidget_accountList->setItemWidget  ( item, widget          );
   listWidget_accountList->setCurrentItem ( item                  );
}

void DlgAccounts::on_button_accountDown_clicked()
{
   qDebug() << "on_button_accountDown_clicked";
   int currentRow = listWidget_accountList->currentRow();
   QListWidgetItem * prevItem = listWidget_accountList->takeItem(currentRow);
   AccountView* account = accountList->getAccountByItem(prevItem);
   //we need to build a new item to set the itemWidget back
   account->initItem();
   QListWidgetItem * item = account->getItem();
   AccountItemWidget * widget = account->getItemWidget();
   accountList->downAccount(currentRow);
   listWidget_accountList->insertItem     ( currentRow + 1 , item );
   listWidget_accountList->setItemWidget  ( item, widget          );
   listWidget_accountList->setCurrentItem ( item                  );
}

void DlgAccounts::on_button_accountAdd_clicked()
{
   qDebug() << "on_button_accountAdd_clicked";
   QString itemName = QInputDialog::getText(this, "New account", "Enter new account's alias");
   itemName = itemName.simplified();
   if (!itemName.isEmpty()) {
      AccountView* account = accountList->addAccount(itemName);
      addAccountToAccountList(account);
      int r = listWidget_accountList->count() - 1;
      listWidget_accountList->setCurrentRow(r);
      frame2_editAccounts->setEnabled(true);
   }
}

void DlgAccounts::on_button_accountRemove_clicked()
{
   qDebug() << "on_button_accountRemove_clicked";
   int r = listWidget_accountList->currentRow();
   QListWidgetItem * item = listWidget_accountList->takeItem(r);
   accountList->removeAccount(item);
   listWidget_accountList->setCurrentRow( (r >= listWidget_accountList->count()) ? r-1 : r );
}

//<<<<<<< HEAD
/*void DlgAccounts::on_toolButton_accountsApply_clicked()
{
   qDebug() << "on_toolButton_accountsApply_clicked";
   updateSettings();
   updateWidgets();
}*/
//=======
// void DlgAccounts::on_toolButton_accountsApply_clicked() //This button have been removed, coded kept for potential reversal
// {
//    qDebug() << "on_toolButton_accountsApply_clicked";
//    updateSettings();
//    updateWidgets();
// }
//>>>>>>> master

void DlgAccounts::on_edit1_alias_textChanged(const QString & text)
{
   qDebug() << "on_edit1_alias_textChanged";
   AccountItemWidget * widget = (AccountItemWidget *) listWidget_accountList->itemWidget(listWidget_accountList->currentItem());
   widget->setAccountText(text);
}

void DlgAccounts::updateAccountListCommands()
{
   qDebug() << "updateAccountListCommands";
   bool buttonsEnabled[4] = {true,true,true,true};
   if(! listWidget_accountList->currentItem()) {
      buttonsEnabled[0] = false;
      buttonsEnabled[1] = false;
      buttonsEnabled[3] = false;
   }
   else if(listWidget_accountList->currentRow() == 0) {
      buttonsEnabled[0] = false;
   }
   if(listWidget_accountList->currentRow() == listWidget_accountList->count() - 1) {
      buttonsEnabled[1] = false;
   }
   
   button_accountUp->setEnabled     ( buttonsEnabled[0] );
   button_accountDown->setEnabled   ( buttonsEnabled[1] );
   button_accountAdd->setEnabled    ( buttonsEnabled[2] );
   button_accountRemove->setEnabled ( buttonsEnabled[3] );
}

void DlgAccounts::updateAccountStates()
{
   qDebug() << "updateAccountStates";
   for (int i = 0; i < accountList->size(); i++) {
      AccountView* current = accountList->getAccountAt(i);
      current->updateState();
   }
   updateStatusLabel(listWidget_accountList->currentItem());
}

void DlgAccounts::updateStatusLabel(QListWidgetItem * item)
{
   if(! item ) {  
          return;  
        }
   AccountView* account = accountList->getAccountByItem(item);
   updateStatusLabel(account);
}

void DlgAccounts::updateStatusLabel(AccountView* account)
{
   if(! account ) {  
          return;  
        }
   QString status = account->getAccountDetail(ACCOUNT_STATUS);
   edit7_state->setText( "<FONT COLOR=\"" + account->getStateColorName() + "\">" + status + "</FONT>" );
}

bool DlgAccounts::hasChanged()
{
   bool res = accountListHasChanged;
   qDebug() << "DlgAccounts::hasChanged " << res;
   return res;
}


void DlgAccounts::updateSettings()
{
   qDebug() << "DlgAccounts::updateSettings";
   if(accountListHasChanged) {
      saveAccountList();
      //toolButton_accountsApply->setEnabled(false);
      accountListHasChanged = false;
   }
}

void DlgAccounts::updateWidgets()
{
   qDebug() << "DlgAccounts::updateWidgets";
   loadAccountList();
   //toolButton_accountsApply->setEnabled(false);
   accountListHasChanged = false;
}

void DlgAccounts::loadCodecList() 
{
  ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
  QVector<int> codecIdList = configurationManager.getAudioCodecList();
  QStringList tmpNameList;
  
  foreach (int aCodec, codecIdList) {
    QStringList codec = configurationManager.getAudioCodecDetails(aCodec);
    QHash<QString, QString> _codec;
    _codec["name"]      = codec[0];
    _codec["frequency"] = codec[1];
    _codec["bitrate"]   = codec[2];
    _codec["id"]        = QString::number(aCodec);
    
    tmpNameList << _codec["name"];
    
    codecList.push_back(_codec);
  }
  
  //Generate a relative alias for each codec
  for (int i =0; i < codecList.size();i++) {
    if (tmpNameList.indexOf(codecList[i]["name"]) == tmpNameList.lastIndexOf(codecList[i]["name"])) {
      codecList[i]["alias"] = codecList[i]["name"];
    }
    else {
      codecList[i]["alias"] = codecList[i]["name"] + " (" + codecList[i]["frequency"] + ")";
    }
  }
}


void DlgAccounts::codecClicked(const QModelIndex& model) 
{
   Q_UNUSED(model)
   foreach (StringHash aCodec, codecList) {
      if (aCodec["alias"] == keditlistbox_codec->currentText()) {
        label_bitrate_value->setText   ( aCodec["bitrate"]   );
        label_frequency_value->setText ( aCodec["frequency"] );
      }
   }
   if (keditlistbox_codec->items().size() == codecList.size())
      keditlistbox_codec->addButton()->setEnabled(false);
   else
      keditlistbox_codec->addButton()->setEnabled(true);
}

void DlgAccounts::addCodec(QString name) 
{
  if (name.isEmpty()) {
    Private_AddCodecDialog* aDialog = new Private_AddCodecDialog(codecList, keditlistbox_codec->items(), this);
    aDialog->show();
    connect(aDialog, SIGNAL(addCodec(QString)), this, SLOT(addCodec(QString)));
  }
  else {
    keditlistbox_codec->insertItem(name);
    accountListHasChanged = true;
    emit updateButtons();
  }
}

void DlgAccounts::codecChanged() 
{
   if (keditlistbox_codec->items().size() == codecList.size())
      keditlistbox_codec->addButton()->setEnabled(false);
   else
      keditlistbox_codec->addButton()->setEnabled(true);
  
   accountListHasChanged = true;
   emit updateButtons();
}

void DlgAccounts::updateCombo(int value) 
{
   Q_UNUSED(value)
   switch (combo_security_STRP->currentIndex()) {
      case 0: //KEY_EXCHANGE_NONE
         checkbox_SDES_fallback_rtp->setVisible   ( false );
         checkbox_ZRTP_Ask_user->setVisible       ( false );
         checkbox_ZRTP_display_SAS->setVisible    ( false );
         checkbox_ZRTP_warn_supported->setVisible ( false );
         checkbox_ZTRP_send_hello->setVisible     ( false );
         break;
      case 1: //ZRTP
         checkbox_SDES_fallback_rtp->setVisible   ( false );
         checkbox_ZRTP_Ask_user->setVisible       ( true  );
         checkbox_ZRTP_display_SAS->setVisible    ( true  );
         checkbox_ZRTP_warn_supported->setVisible ( true  );
         checkbox_ZTRP_send_hello->setVisible     ( true  );
         break;
      case 2: //SDES
         checkbox_SDES_fallback_rtp->setVisible   ( true  );
         checkbox_ZRTP_Ask_user->setVisible       ( false );
         checkbox_ZRTP_display_SAS->setVisible    ( false );
         checkbox_ZRTP_warn_supported->setVisible ( false );
         checkbox_ZTRP_send_hello->setVisible     ( false );
         break;
   }
}


void DlgAccounts::loadCredentails(QString accountId) {
   credentialInfo.clear();
   list_credential->clear();
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   VectorMapStringString credentials = configurationManager.getCredentials(accountId);
   for (int i=0; i < credentials.size(); i++) {
      QListWidgetItem* newItem = new QListWidgetItem();
      newItem->setText(credentials[i]["username"]);
      CredentialData data;
      data.pointer  = newItem                    ;
      data.name     = credentials[i]["username"] ;
      data.password = credentials[i]["password"] ;
      data.realm    = credentials[i]["realm"]    ;
      credentialInfo[newItem] = data;
      credentialList << data;
      list_credential->addItem(newItem);
   }
}

void DlgAccounts::saveCredential(QString accountId) {
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   //configurationManager.setNumberOfCredential(accountId, list_credential->count()); //TODO
   VectorMapStringString toReturn;
   
   for (int i=0; i < list_credential->count();i++) {
      QListWidgetItem* currentItem = list_credential->item(i);
      MapStringString credentialData;
      credentialData["username"] = credentialInfo[currentItem].name     ;
      credentialData["password"] = credentialInfo[currentItem].password ;
      credentialData["realm"]    = credentialInfo[currentItem].realm    ;
      toReturn << credentialData;
   }
   configurationManager.setCredentials(accountId,toReturn);
}

void DlgAccounts::addCredential() {
   QListWidgetItem* newItem = new QListWidgetItem();
   newItem->setText("New credential");
   credentialInfo[newItem] = {newItem, "New credential", "",""};

   selectCredential(newItem,list_credential->currentItem());
   list_credential->addItem(newItem);
   list_credential->setCurrentItem(newItem);
}

void DlgAccounts::selectCredential(QListWidgetItem* item, QListWidgetItem* previous) {
   if (previous) {
      credentialInfo[previous].realm    = edit_credential_realm->text();
      credentialInfo[previous].name     = edit_credential_auth->text();
      credentialInfo[previous].password = edit_credential_password->text();
      previous->setText(edit_credential_auth->text());
   }
   list_credential->setCurrentItem      ( item                          );
   edit_credential_realm->setText       ( credentialInfo[item].realm    );
   edit_credential_auth->setText        ( credentialInfo[item].name     );
   edit_credential_password->setText    ( credentialInfo[item].password );
   edit_credential_realm->setEnabled    ( true                          );
   edit_credential_auth->setEnabled     ( true                          );
   edit_credential_password->setEnabled ( true                          );
}

void DlgAccounts::removeCredential() {
   list_credential->takeItem(list_credential->currentRow());
}

void DlgAccounts::enablePublished()
{
   lineEdit_pa_published_address->setDisabled(radioButton_pa_same_as_local->isChecked());
   spinBox_pa_published_port->setDisabled(radioButton_pa_same_as_local->isChecked());
}
