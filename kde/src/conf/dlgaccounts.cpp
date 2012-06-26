/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/

#include "dlgaccounts.h"


//Qt
#include <QtCore/QString>
#include <QtGui/QInputDialog>
#include <QtGui/QTableWidget>
#include <QtGui/QListWidgetItem>
#include <QtGui/QWidget>

//KDE
#include <KConfigDialog>
#include <KDebug>
#include <KStandardDirs>

//SFLPhone
#include "conf/ConfigurationDialog.h"
#include "lib/configurationmanager_interface_singleton.h"
#include "SFLPhoneView.h"
#include "lib/sflphone_const.h"

Private_AddCodecDialog::Private_AddCodecDialog(QList< StringHash > itemList, QStringList currentItems ,QWidget* parent) : KDialog(parent)
{
   codecTable = new QTableWidget(this);
   codecTable->verticalHeader()->setVisible(false);
   codecTable->setColumnCount(4);
   for (int i=0;i<4;i++) {
      codecTable->setHorizontalHeaderItem( i, new QTableWidgetItem(0));
      codecTable->horizontalHeader()->setResizeMode(i,QHeaderView::ResizeToContents);
   }

   codecTable->setSelectionBehavior(QAbstractItemView::SelectRows);
   codecTable->horizontalHeader()->setResizeMode(0,QHeaderView::Stretch);
   codecTable->horizontalHeaderItem(0)->setText( "Name"      );
   codecTable->horizontalHeaderItem(1)->setText( "Bitrate"   );
   codecTable->horizontalHeaderItem(2)->setText( "Frequency" );
   codecTable->horizontalHeaderItem(3)->setText( "Alias"     );
   int i =0;
   foreach (StringHash aCodec, itemList) {
      if ( currentItems.indexOf(aCodec["alias"]) == -1) {
         codecTable->setRowCount(i+1);
         QTableWidgetItem* cName       = new QTableWidgetItem( aCodec["name"]      );
         codecTable->setItem( i,0,cName      );
         QTableWidgetItem* cBitrate    = new QTableWidgetItem( aCodec["bitrate"]   );
         codecTable->setItem( i,1,cBitrate   );
         QTableWidgetItem* cFrequency  = new QTableWidgetItem( aCodec["frequency"] );
         codecTable->setItem( i,2,cFrequency );
         QTableWidgetItem* cAlias      = new QTableWidgetItem( aCodec["alias"]     );
         codecTable->setItem( i,3,cAlias     );
         i++;
      }
   }
   setMainWidget(codecTable);
   resize(550,300);
   connect(this, SIGNAL(okClicked()), this, SLOT(emitNewCodec()));
} //Private_AddCodecDialog

///When a new codec is added (ok pressed)
void Private_AddCodecDialog::emitNewCodec() {
   if (codecTable->currentRow() >= 0)
   emit addCodec(codecTable->item(codecTable->currentRow(),3)->text());
}

///Constructor
DlgAccounts::DlgAccounts(KConfigDialog* parent)
 : QWidget(parent)/*,accountList(NULL)*/
{
   setupUi(this);
   disconnect(keditlistbox_codec->addButton(),SIGNAL(clicked()));
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   button_accountUp->setIcon         ( KIcon( "go-up"       ) );
   button_accountDown->setIcon       ( KIcon( "go-down"     ) );
   button_accountAdd->setIcon        ( KIcon( "list-add"    ) );
   button_accountRemove->setIcon     ( KIcon( "list-remove" ) );
   button_add_credential->setIcon    ( KIcon( "list-add"    ) );
   button_remove_credential->setIcon ( KIcon( "list-remove" ) );
   listView_accountList->setModel(AccountList::getInstance());

   m_pRingTonePath->setMode(KFile::File | KFile::ExistingOnly);
   m_pRingTonePath->lineEdit()->setObjectName("m_pRingTonePath");
   m_pRingTonePath->lineEdit()->setReadOnly(true);

   //accountList = new ConfigAccountList(false);
   loadAccountList();
   loadCodecList();
   accountListHasChanged = false;

   //SLOTS
   //                     SENDER                            SIGNAL                    RECEIVER               SLOT                   /
   /**/connect(edit1_alias,                    SIGNAL(textEdited(const QString &))  , this      , SLOT(changedAccountList()        ));
   /**/connect(edit2_protocol,                 SIGNAL(activated(int))               , this      , SLOT(changedAccountList()        ));
   /**/connect(edit3_server,                   SIGNAL(textEdited(const QString &))  , this      , SLOT(changedAccountList()        ));
   /**/connect(edit4_user,                     SIGNAL(textEdited(const QString &))  , this      , SLOT(changedAccountList()        ));
   /**/connect(edit5_password,                 SIGNAL(textEdited(const QString &))  , this      , SLOT(changedAccountList()        ));
   /**/connect(edit6_mailbox,                  SIGNAL(textEdited(const QString &))  , this      , SLOT(changedAccountList()        ));
   /**/connect(spinbox_regExpire,              SIGNAL(editingFinished())            , this      , SLOT(changedAccountList()        ));
   /**/connect(comboBox_ni_local_address,      SIGNAL(currentIndexChanged (int))    , this      , SLOT(changedAccountList()        ));
   /**/connect(button_accountUp,               SIGNAL(clicked())                    , this      , SLOT(changedAccountList()        ));
   /**/connect(button_accountDown,             SIGNAL(clicked())                    , this      , SLOT(changedAccountList()        ));
   /**/connect(button_accountAdd,              SIGNAL(clicked())                    , this      , SLOT(changedAccountList()        ));
   /**/connect(button_accountRemove,           SIGNAL(clicked())                    , this      , SLOT(changedAccountList()        ));
   /**/connect(edit_tls_private_key_password,  SIGNAL(textEdited(const QString &))  , this      , SLOT(changedAccountList()        ));
   /**/connect(spinbox_tls_listener,           SIGNAL(editingFinished())            , this      , SLOT(changedAccountList()        ));
   /**/connect(file_tls_authority,             SIGNAL(textChanged(const QString &)) , this      , SLOT(changedAccountList()        ));
   /**/connect(file_tls_endpoint,              SIGNAL(textChanged(const QString &)) , this      , SLOT(changedAccountList()        ));
   /**/connect(file_tls_private_key,           SIGNAL(textChanged(const QString &)) , this      , SLOT(changedAccountList()        ));
   /**/connect(combo_tls_method,               SIGNAL(currentIndexChanged(int))     , this      , SLOT(changedAccountList()        ));
   /**/connect(edit_tls_cipher,                SIGNAL(textEdited(const QString &))  , this      , SLOT(changedAccountList()        ));
   /**/connect(edit_tls_outgoing,              SIGNAL(textEdited(const QString &))  , this      , SLOT(changedAccountList()        ));
   /**/connect(spinbox_tls_timeout_sec,        SIGNAL(editingFinished())            , this      , SLOT(changedAccountList()        ));
   /**/connect(spinbox_tls_timeout_msec,       SIGNAL(editingFinished())            , this      , SLOT(changedAccountList()        ));
   /**/connect(check_tls_incoming,             SIGNAL(clicked(bool))                , this      , SLOT(changedAccountList()        ));
   /**/connect(check_tls_answer,               SIGNAL(clicked(bool))                , this      , SLOT(changedAccountList()        ));
   /**/connect(check_tls_requier_cert,         SIGNAL(clicked(bool))                , this      , SLOT(changedAccountList()        ));
   /**/connect(group_security_tls,             SIGNAL(clicked(bool))                , this      , SLOT(changedAccountList()        ));
   /**/connect(radioButton_pa_same_as_local,   SIGNAL(clicked(bool))                , this      , SLOT(changedAccountList()        ));
   /**/connect(radioButton_pa_custom,          SIGNAL(clicked(bool))                , this      , SLOT(changedAccountList()        ));
   /**/connect(m_pRingtoneListLW,              SIGNAL(currentRowChanged(int))       , this      , SLOT(changedAccountList()        ));
   /**/connect(m_pUseCustomFileCK,             SIGNAL(clicked(bool))                , this      , SLOT(changedAccountList()        ));
   /**/connect(m_pCodecsLW,                    SIGNAL(itemChanged(QListWidgetItem*)), this      , SLOT(changedAccountList()        ));
   /**/connect(m_pCodecsLW,                    SIGNAL(currentTextChanged(QString))  , this      , SLOT(loadVidCodecDetails(QString)));
   /**/connect(&configurationManager,          SIGNAL(accountsChanged())            , this      , SLOT(updateAccountStates()       ));
   /**/connect(edit_tls_private_key_password,  SIGNAL(textEdited(const QString &))  , this      , SLOT(changedAccountList()        ));
   /**/connect(this,                           SIGNAL(updateButtons())              , parent    , SLOT(updateButtons()             ));
   /**/connect(keditlistbox_codec->listView(), SIGNAL(clicked(QModelIndex))         , this      , SLOT(codecClicked(QModelIndex)   ));
   /**/connect(keditlistbox_codec->addButton(),SIGNAL(clicked())                    , this      , SLOT(addCodec()                  ));
   /**/connect(keditlistbox_codec,             SIGNAL(changed())                    , this      , SLOT(codecChanged()              ));
   /**/connect(combo_security_STRP,            SIGNAL(currentIndexChanged(int))     , this      , SLOT(updateCombo(int)            ));
   /**/connect(button_add_credential,          SIGNAL(clicked())                    , this      , SLOT(addCredential()             ));
   /**/connect(button_remove_credential,       SIGNAL(clicked())                    , this      , SLOT(removeCredential()          ));
   /*                                                                                                                               */

   connect(listView_accountList, SIGNAL(currentAccountChanged(Account*,Account*)), this, SLOT(accountListChanged(Account*,Account*)));
   connect(listView_accountList, SIGNAL(currentIndexChanged(QModelIndex,QModelIndex)), this, SLOT(updateAccountListCommands()));
   connect(list_credential,      SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), this, SLOT(selectCredential  (QListWidgetItem*,QListWidgetItem*)));

   //Disable control
   connect(radioButton_pa_same_as_local,   SIGNAL(clicked(bool))               , this   , SLOT(enablePublished()));
   connect(radioButton_pa_custom,          SIGNAL(clicked(bool))               , this   , SLOT(enablePublished()));
} //DlgAccounts

///Destructor
DlgAccounts::~DlgAccounts()
{
   //accountList->disconnect();
   //if (accountList) delete accountList;
}

///Save the account list, necessary for new and removed accounts
void DlgAccounts::saveAccountList()
{
   disconnectAccountsChangedSignal();
   //save the account being edited
   if(listView_accountList->currentIndex().isValid()) {
      saveAccount(listView_accountList->currentIndex());
   }
   
   //accountList->save();
   AccountList::getInstance()->save();

//    ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
//    //save the account being edited
//    if(listView_accountList->currentIndex()) {
//       saveAccount(listView_accountList->currentIndex());
//    }
//    QStringList accountIds= QStringList(configurationManager.getAccountList().value());
// 
//    //create or update each account from accountList
//    for (int i = 0; i < accountList->size(); i++) {
//       AccountView* current = (*accountList)[i];
//       QString currentId;
//       current->save();
//       currentId = QString(current->getAccountId());
//    }
// 
//    //remove accounts that are in the configurationManager but not in the client
//    for (int i = 0; i < accountIds.size(); i++) {
//       if(!accountList->getAccountById(accountIds[i])) {
//          configurationManager.removeAccount(accountIds[i]);
//       }
//    }
// 
//    configurationManager.setAccountsOrder(accountList->getOrderedList());
   connectAccountsChangedSignal();
} //saveAccountList

void DlgAccounts::connectAccountsChangedSignal()
{
   kDebug() << "connectAccountsChangedSignal";
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   connect(&configurationManager, SIGNAL(accountsChanged()),
           this,                  SLOT(updateAccountStates()));
}

void DlgAccounts::disconnectAccountsChangedSignal()
{
   kDebug() << "disconnectAccountsChangedSignal";
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   disconnect(&configurationManager, SIGNAL(accountsChanged()),
           this,                  SLOT(updateAccountStates()));
}

///Save an account using the values from the widgets
void DlgAccounts::saveAccount(QModelIndex item)
{
   QString protocolsTab[] = ACCOUNT_TYPES_TAB;

   if(!item.isValid()) {
      kDebug() << "Attempting to save details of an account from a NULL item";
      return;
   }

   Account* account = AccountList::getInstance()->getAccountByModelIndex(item);
   if(!account) {
      kDebug() << "Attempting to save details of an unexisting account : " << item.data(Qt::DisplayRole);
      return;
   }
   
   //ACCOUNT DETAILS
   //                                                                     WIDGET VALUE                                     /
   /**/account->setAccountAlias                ( edit1_alias->text()                                                      );
   /**/account->setAccountType                 ( protocolsTab[edit2_protocol->currentIndex()]                             );
   /**/account->setAccountHostname             ( edit3_server->text()                                                     );
   /**/account->setAccountUsername             ( edit4_user->text()                                                       );
   /**/account->setAccountPassword             ( edit5_password->text()                                                   );
   /**/account->setAccountMailbox              ( edit6_mailbox->text()                                                    );
   /**/account->setAccountEnabled              ( item.data(Qt::CheckStateRole).toBool()                                   );
   /**/account->setAccountRegistrationExpire   ( spinbox_regExpire->value()                                               );
   /**/                                                                                                                 /**/
   /*                                            Security                                                                 */
   /**/account->setTlsPassword                 ( edit_tls_private_key_password->text()                                    );
   /**/account->setTlsListenerPort             ( spinbox_tls_listener->value()                                            );
   /**/account->setTlsCaListFile               ( file_tls_authority->text()                                               );
   /**/account->setTlsCertificateFile          ( file_tls_endpoint->text()                                                );
   /**/account->setTlsPrivateKeyFile           ( file_tls_private_key->text()                                             );
   /**/account->setTlsMethod                   ( combo_tls_method->currentIndex()                                         );
   /**/account->setTlsCiphers                  ( edit_tls_cipher->text()                                                  );
   /**/account->setTlsServerName               ( edit_tls_outgoing->text()                                                );
   /**/account->setTlsNegotiationTimeoutSec    ( spinbox_tls_timeout_sec->value()                                         );
   /**/account->setTlsNegotiationTimeoutMsec   ( spinbox_tls_timeout_msec->value()                                        );
   ///**/account->setTlsMethod                   ( QString::number(combo_security_STRP->currentIndex())                     );
   /**/account->setTlsVerifyServer             ( check_tls_incoming->isChecked()                                          );
   /**/account->setTlsVerifyClient             ( check_tls_answer->isChecked()                                            );
   /**/account->setTlsRequireClientCertificate ( check_tls_requier_cert->isChecked()                                      );
   /**/account->setTlsEnable                   ( group_security_tls->isChecked()                                          );
   /**/account->setAccountDisplaySasOnce       ( checkbox_ZRTP_Ask_user->isChecked()                                      );
   /**/account->setAccountSrtpRtpFallback      ( checkbox_SDES_fallback_rtp->isChecked()                                  );
   /**/account->setAccountZrtpDisplaySas       ( checkbox_ZRTP_display_SAS->isChecked()                                   );
   /**/account->setAccountZrtpNotSuppWarning   ( checkbox_ZRTP_warn_supported->isChecked()                                );
   /**/account->setAccountZrtpHelloHash        ( checkbox_ZTRP_send_hello->isChecked()                                    );
   /**/account->setAccountSipStunEnabled       ( checkbox_stun->isChecked()                                               );
   /**/account->setPublishedSameAsLocal        ( radioButton_pa_same_as_local->isChecked()                                );
   /**/account->setAccountSipStunServer        ( line_stun->text()                                                        );
   /**/account->setPublishedPort               ( spinBox_pa_published_port->value()                                       );
   /**/account->setPublishedAddress            ( lineEdit_pa_published_address ->text()                                   );
   /**/account->setLocalPort                   ( spinBox_pa_published_port->value()                                       );
   /**/account->setLocalInterface              ( comboBox_ni_local_address->currentText()                                 );
   /**/account->setRingtoneEnabled             ( m_pEnableRingtoneGB->isChecked()                                         );
   /**/account->setRingtonePath                ( m_pRingTonePath->url().path()                                            );
   //                                                                                                                      /

   QStringList _codecList;
   foreach (QString aCodec, keditlistbox_codec->items()) {
      foreach (StringHash _aCodec, codecList) {
         if (_aCodec["alias"] == aCodec) {
            _codecList << _aCodec["id"];
         }
      }
   }

   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   configurationManager.setActiveAudioCodecList(_codecList, account->getAccountId());
   kDebug() << "Account codec have been saved" << _codecList << account->getAccountId();

   if (m_pRingtoneListLW->selectedItems().size() == 1 && m_pRingtoneListLW->currentIndex().isValid() ) {
      QListWidgetItem* selectedRingtone = m_pRingtoneListLW->currentItem();
      RingToneListItem* ringtoneWidget = qobject_cast<RingToneListItem*>(m_pRingtoneListLW->itemWidget(selectedRingtone));
      if (ringtoneWidget) {
         account->setRingtonePath(ringtoneWidget->m_Path);
      }
   }

   QStringList activeCodecs;
   for (int i=0;i < m_pCodecsLW->count();i++) {
      QListWidgetItem* item = m_pCodecsLW->item(i);
      if (item->checkState() == Qt::Checked) {
         activeCodecs << item->text();
      }
   }
   VideoCodec::setActiveCodecList(account,activeCodecs);

   saveCredential(account->getAccountId());
} //saveAccount

void DlgAccounts::loadAccount(QModelIndex item)
{
   if(! item.isValid() ) {
      kDebug() << "Attempting to load details of an account from a NULL item";
      return;
   }

   Account* account = AccountList::getInstance()->getAccountByModelIndex(item);
   if(! account ) {
      kDebug() << "Attempting to load details of an unexisting account";
      return;
   }

   edit1_alias->setText( account->getAccountAlias());

   QString protocolsTab[] = ACCOUNT_TYPES_TAB;
   QList<QString> * protocolsList = new QList<QString>();
   for(int i = 0 ; i < (int) (sizeof(protocolsTab) / sizeof(QString)) ; i++) {
      protocolsList->append(protocolsTab[i]);
   }

   QString accountName = account->getAccountType();
   int protocolIndex = protocolsList->indexOf(accountName);
   delete protocolsList;



   loadCredentails(account->getAccountId());

   if (credentialList.size() > 0) {
      bool found = false;
      foreach(CredentialData data,credentialList) {
         if (data.name == account->getAccountUsername()) {
            edit5_password->setText( data.password );
            found = true;
         }
      }
      if (!found) {
         //Better than nothing, can happen if username change
         edit5_password->setText( credentialList[0].password );
      }
   }
   else {
      edit5_password->setText("");
   }


   switch (account->getTlsMethod()) {
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
   
   //         WIDGET VALUE                                             VALUE                 /
   /**/edit2_protocol->setCurrentIndex          ( (protocolIndex < 0) ? 0 : protocolIndex    );
   /**/edit3_server->setText                    (  account->getAccountHostname             ());
   /**/edit4_user->setText                      (  account->getAccountUsername             ());
   /**/edit6_mailbox->setText                   (  account->getAccountMailbox              ());
   /**/checkbox_ZRTP_Ask_user->setChecked       (  account->isAccountDisplaySasOnce        ());
   /**/checkbox_SDES_fallback_rtp->setChecked   (  account->isAccountSrtpRtpFallback       ());
   /**/checkbox_ZRTP_display_SAS->setChecked    (  account->isAccountZrtpDisplaySas        ());
   /**/checkbox_ZRTP_warn_supported->setChecked (  account->isAccountZrtpNotSuppWarning    ());
   /**/checkbox_ZTRP_send_hello->setChecked     (  account->isAccountZrtpHelloHash         ());
   /**/checkbox_stun->setChecked                (  account->isAccountSipStunEnabled        ());
   /**/line_stun->setText                       (  account->getAccountSipStunServer        ());
   /**/spinbox_regExpire->setValue              (  account->getAccountRegistrationExpire   ());
   /**/radioButton_pa_same_as_local->setChecked (  account->isPublishedSameAsLocal         ());
   /**/radioButton_pa_custom->setChecked        ( !account->isPublishedSameAsLocal         ());
   /**/lineEdit_pa_published_address->setText   (  account->getPublishedAddress            ());
   /**/spinBox_pa_published_port->setValue      (  account->getPublishedPort               ());
   /*                                                  Security                             **/
   /**/edit_tls_private_key_password->setText   (  account->getTlsPassword                 ());
   /**/spinbox_tls_listener->setValue           (  account->getTlsListenerPort             ());
   /**/file_tls_authority->setText              (  account->getTlsCaListFile               ());
   /**/file_tls_endpoint->setText               (  account->getTlsCertificateFile          ());
   /**/file_tls_private_key->setText            (  account->getTlsPrivateKeyFile           ());
   /**/edit_tls_cipher->setText                 (  account->getTlsCiphers                  ());
   /**/edit_tls_outgoing->setText               (  account->getTlsServerName               ());
   /**/spinbox_tls_timeout_sec->setValue        (  account->getTlsNegotiationTimeoutSec    ());
   /**/spinbox_tls_timeout_msec->setValue       (  account->getTlsNegotiationTimeoutMsec   ());
   /**/check_tls_incoming->setChecked           (  account->isTlsVerifyServer              ());
   /**/check_tls_answer->setChecked             (  account->isTlsVerifyClient              ());
   /**/check_tls_requier_cert->setChecked       (  account->isTlsRequireClientCertificate  ());
   /**/group_security_tls->setChecked           (  account->isTlsEnable                    ());
   /**/combo_security_STRP->setCurrentIndex     (  account->getTlsMethod                   ());
   /*                                                                                       */

   if (account->getAccountAlias() == "IP2IP") {
      frame2_editAccounts->setTabEnabled(0,false);
      frame2_editAccounts->setTabEnabled(1,false);
      frame2_editAccounts->setTabEnabled(3,false);
      frame2_editAccounts->setTabEnabled(4,false);
   }
   else {
      frame2_editAccounts->setTabEnabled(0,true);
      frame2_editAccounts->setTabEnabled(1,true);
      frame2_editAccounts->setTabEnabled(3,true);
      frame2_editAccounts->setTabEnabled(4,true);
      frame2_editAccounts->setCurrentIndex(0);
   }

   m_pEnableRingtoneGB->setChecked(account->isRingtoneEnabled());
   QString ringtonePath = KStandardDirs::realFilePath(account->getRingtonePath());
   m_pRingTonePath->setUrl( ringtonePath );


   combo_tls_method->setCurrentIndex        ( account->getTlsMethod() );
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();

   m_pRingtoneListLW->clear();
   m_hRingtonePath = configurationManager.getRingtoneList();
   QMutableMapIterator<QString, QString> iter(m_hRingtonePath);
   bool found = false;
   while (iter.hasNext()) {
      iter.next();
      QListWidgetItem* item = new QListWidgetItem();
      RingToneListItem* item_widget = new RingToneListItem(iter.key(),iter.value());
      m_pRingtoneListLW->addItem(item);
      m_pRingtoneListLW->setItemWidget(item,item_widget);
      if (KStandardDirs::realFilePath(iter.key()) == ringtonePath) {
         m_pUseCustomFileCK->setChecked(false);
         m_pRingTonePath->setDisabled(true);
         item->setSelected(true);
         found = true;
      }
   }
   if (!found) m_pRingtoneListLW->setDisabled(true);

   #ifdef ENABLE_VIDEO
      m_pCodecsLW->clear();
      QList<VideoCodec*> codecs       = VideoCodec::getCodecList();
      QList<VideoCodec*> activeCodecs = VideoCodec::getActiveCodecList(account);
      foreach(VideoCodec* codec,codecs) {
         if (codec) {
            QListWidgetItem* i = new QListWidgetItem(codec->getName());
            i->setCheckState((activeCodecs.indexOf(codec) != -1)?Qt::Checked:Qt::Unchecked);
            m_pCodecsLW->addItem(i);
         }
      }
   #else
      m_pVideoCodecGB->setVisible(false);
   #endif
   
   comboBox_ni_local_address->clear();
   QStringList interfaceList = configurationManager.getAllIpInterfaceByName();
   comboBox_ni_local_address->addItems(interfaceList);

   spinBox_ni_local_port->setValue(account->getLocalPort());
   comboBox_ni_local_address->setCurrentIndex(comboBox_ni_local_address->findText(account->getLocalInterface())); //TODO need to load the list first

   QVector<int> activeCodecList = configurationManager.getActiveAudioCodecList(account->getAccountId());
   keditlistbox_codec->clear();
   foreach (int aCodec, activeCodecList) {
      foreach (StringHash _aCodec, codecList) {
         if (_aCodec["id"] == QString::number(aCodec))
            keditlistbox_codec->insertItem(_aCodec["alias"]);
      }
   }



   if(protocolIndex == 0) { // if sip selected
      checkbox_stun->setChecked(account->isAccountSipStunEnabled());
      line_stun->setText( account->getAccountSipStunServer() );
      //checkbox_zrtp->setChecked(account->getAccountDetail(ACCOUNT_SRTP_ENABLED) == REGISTRATION_ENABLED_TRUE);

      tab_advanced->setEnabled(true);
      line_stun->setEnabled(checkbox_stun->isChecked());
      radioButton_pa_same_as_local->setDisabled(checkbox_stun->isChecked());
      radioButton_pa_custom->setDisabled(checkbox_stun->isChecked());
   }
   else {
      checkbox_stun->setChecked(false);
      line_stun->setText( account->getAccountSipStunServer() );
      //checkbox_zrtp->setChecked(false);

      tab_advanced->setEnabled(false);
   }

   updateStatusLabel(account);
   enablePublished();
   frame2_editAccounts->setEnabled(true);
} //loadAccount

///Load an account
void DlgAccounts::loadAccountList()
{
   AccountList::getInstance()->updateAccounts();
   //TODO listView_accountList->clear();
   for (int i = 0; i < AccountList::getInstance()->size(); ++i) {
      addAccountToAccountList((*AccountList::getInstance())[i]);
   }
   if (listView_accountList->model()->rowCount() > 0 && !listView_accountList->currentIndex().isValid())
      listView_accountList->setCurrentIndex(listView_accountList->model()->index(0,0));
   else
      frame2_editAccounts->setEnabled(false);
}

///Add an account to the list
void DlgAccounts::addAccountToAccountList(Account* account)
{
   Q_UNUSED(account)
}

///Called when one of the child widget is modified
void DlgAccounts::changedAccountList()
{
   accountListHasChanged = true;
   emit updateButtons();
}

///Callback when the account change
void DlgAccounts::accountListChanged(Account* current, Account* previous)
{
   kDebug() << "on_listView_accountList_currentItemChanged";
   saveAccount(previous->getIndex());
   
   loadAccount(current->getIndex());
   //updateAccountListCommands();
}

void DlgAccounts::on_button_accountUp_clicked()
{
   kDebug() << "on_button_accountUp_clicked";
   QModelIndex index = listView_accountList->currentIndex();
   Account* acc = AccountList::getInstance()->getAccountByModelIndex(index);
   AccountList::getInstance()->accountUp(index.row());
   listView_accountList->setCurrentIndex(acc->getIndex());
}

void DlgAccounts::on_button_accountDown_clicked()
{
   kDebug() << "on_button_accountDown_clicked";
   QModelIndex index = listView_accountList->currentIndex();
   Account* acc = AccountList::getInstance()->getAccountByModelIndex(index);
   AccountList::getInstance()->accountDown(index.row());
   listView_accountList->setCurrentIndex(acc->getIndex());
}

void DlgAccounts::on_button_accountAdd_clicked()
{
   kDebug() << "on_button_accountAdd_clicked";
   QString itemName = QInputDialog::getText(this, "New account", "Enter new account's alias");
   itemName = itemName.simplified();
   if (!itemName.isEmpty()) {
      AccountList::getInstance()->addAccount(itemName);
      int r = listView_accountList->model()->rowCount() - 1;
      listView_accountList->setCurrentIndex(listView_accountList->model()->index(r,0));
      frame2_editAccounts->setEnabled(true);
   }
} //on_button_accountAdd_clicked

void DlgAccounts::on_button_accountRemove_clicked()
{
   kDebug() << "on_button_accountRemove_clicked";
   AccountList::getInstance()->removeAccount(listView_accountList->currentIndex());
   listView_accountList->setCurrentIndex(listView_accountList->model()->index(0,0));
}

void DlgAccounts::on_edit1_alias_textChanged(const QString & text)
{
   Q_UNUSED(text);
   //TODO reimplement
   kDebug() << "on_edit1_alias_textChanged";
}

void DlgAccounts::updateAccountListCommands()
{
   kDebug() << "updateAccountListCommands";
   bool buttonsEnabled[4] = {true,true,true,true};
   if(! listView_accountList->currentIndex().isValid()) {
      buttonsEnabled[0] = false;
      buttonsEnabled[1] = false;
      buttonsEnabled[3] = false;
   }
   else if(listView_accountList->currentIndex().row() == 0) {
      buttonsEnabled[0] = false;
   }
   if(listView_accountList->currentIndex().row() == listView_accountList->model()->rowCount() - 1) {
      buttonsEnabled[1] = false;
   }

   button_accountUp->setEnabled     ( buttonsEnabled[0] );
   button_accountDown->setEnabled   ( buttonsEnabled[1] );
   button_accountAdd->setEnabled    ( buttonsEnabled[2] );
   button_accountRemove->setEnabled ( buttonsEnabled[3] );
}

void DlgAccounts::loadVidCodecDetails(const QString& text)
{
   VideoCodec* codec = VideoCodec::getCodec(text);
   if (codec)
      m_pBitrateL->setText(codec->getBitrate());
}

void DlgAccounts::updateAccountStates()
{
   kDebug() << "updateAccountStates";
   for (int i = 0; i < AccountList::getInstance()->size(); i++) {
      Account* current = AccountList::getInstance()->getAccountAt(i);
      current->updateState();
   }
   updateStatusLabel(listView_accountList->currentIndex());
}

void DlgAccounts::updateStatusLabel(QModelIndex item)
{
   kDebug() << "MODEL index is" << item.row();
   if(!item.isValid()) {
      return;
   }
   Account* account = AccountList::getInstance()->getAccountByModelIndex(item);
   if (account)
      updateStatusLabel(account);
}

void DlgAccounts::updateStatusLabel(Account* account)
{
   if(! account ) {
          return;
        }
   QString status = account->getAccountRegistrationStatus();
   edit7_state->setText( "<FONT COLOR=\"" + account->getStateColorName() + "\">" + status + "</FONT>" );
}

///Have the account changed
bool DlgAccounts::hasChanged()
{
   return accountListHasChanged;
}

///Save settings
void DlgAccounts::updateSettings()
{
   if(accountListHasChanged) {
      saveAccountList();
      //toolButton_accountsApply->setEnabled(false);
      accountListHasChanged = false;
   }
}

///Reload
void DlgAccounts::updateWidgets()
{
   loadAccountList();
   //toolButton_accountsApply->setEnabled(false);
   accountListHasChanged = false;
}

///Get the codecs
void DlgAccounts::loadCodecList()
{
  ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
  QVector<int> codecIdList = configurationManager.getAudioCodecList();
  QStringList tmpNameList;

  foreach (int aCodec, codecIdList) {
    QStringList codec = configurationManager.getAudioCodecDetails(aCodec);
    QHash<QString, QString> _codec;
    _codec[ "name"      ] = codec[0];
    _codec[ "frequency" ] = codec[1];
    _codec[ "bitrate"   ] = codec[2];
    _codec[ "id"        ] = QString::number(aCodec);

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
} //loadCodecList


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
} //codecClicked

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
} //addCodec

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
} //updateCombo


void DlgAccounts::loadCredentails(QString accountId) {
   credentialInfo.clear();
   credentialList.clear();
   list_credential->clear();
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   VectorMapStringString credentials = configurationManager.getCredentials(accountId);
   for (int i=0; i < credentials.size(); i++) {
      QListWidgetItem* newItem = new QListWidgetItem();
      newItem->setText(credentials[i][ CONFIG_ACCOUNT_USERNAME ]);
      CredentialData data;
      data.pointer  = newItem                       ;
      data.name     = credentials[i][ CONFIG_ACCOUNT_USERNAME  ] ;
      data.password = credentials[i][ CONFIG_ACCOUNT_PASSWORD  ] ;
      data.realm    = credentials[i][ CONFIG_ACCOUNT_REALM     ] ;
      credentialInfo[newItem] = data;
      credentialList << data;
      list_credential->addItem(newItem);
   }
} //loadCredentails

void DlgAccounts::saveCredential(QString accountId) {
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   //configurationManager.setNumberOfCredential(accountId, list_credential->count()); //TODO
   VectorMapStringString toReturn;

   for (int i=0; i < list_credential->count();i++) {
      QListWidgetItem* currentItem = list_credential->item(i);
      MapStringString credentialData;
      credentialData[CONFIG_ACCOUNT_USERNAME] = credentialInfo[currentItem].name     ;
      credentialData[CONFIG_ACCOUNT_PASSWORD] = credentialInfo[currentItem].password ;
      credentialData[CONFIG_ACCOUNT_REALM]    = credentialInfo[currentItem].realm    ;
      toReturn << credentialData;
   }
   configurationManager.setCredentials(accountId,toReturn);
} //saveCredential

void DlgAccounts::addCredential() {
   QListWidgetItem* newItem = new QListWidgetItem();
   newItem->setText(i18n("New credential"));
   credentialInfo[newItem] = {newItem, i18n("New credential"), "",""};

   selectCredential(newItem,list_credential->currentItem());
   list_credential->addItem(newItem);
   list_credential->setCurrentItem(newItem);
} //addCredential

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
} //selectCredential

void DlgAccounts::removeCredential() {
   list_credential->takeItem(list_credential->currentRow());
}

void DlgAccounts::enablePublished()
{
   lineEdit_pa_published_address->setDisabled(radioButton_pa_same_as_local->isChecked());
   spinBox_pa_published_port->setDisabled(radioButton_pa_same_as_local->isChecked());
}

//#include <dlgaccount.moc>