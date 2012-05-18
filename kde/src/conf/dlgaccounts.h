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

#ifndef DLGACCOUNTS_H
#define DLGACCOUNTS_H

#include "ui_dlgaccountsbase.h"
#include "AccountView.h"
#include "ConfigAccountList.h"

//Qt
class QTableWidget;
class QListWidgetItem;
class QWidget;

//KDE
class KConfigDialog;

///@struct CredentialData store credential informations
struct CredentialData {
   QListWidgetItem* pointer ;
   QString          name    ;
   QString          password;
   QString          realm   ;
};

//Typedef
typedef QHash<QString, QString> StringHash;                          //Needed to fix a Qt foreach macro argument parsing bug
typedef QHash<QListWidgetItem*, CredentialData> QListWidgetItemHash; //Needed to fix a Qt foreach macro argument parsing bug
typedef QList<CredentialData> CredentialList;

///@class Private_AddCodecDialog Little dialog to add codec to the list
class Private_AddCodecDialog : public KDialog {
  Q_OBJECT
  public:
    Private_AddCodecDialog(QList< StringHash > itemList, QStringList currentItems ,QWidget* parent = 0);

  private:
    QTableWidget* codecTable;

  private slots:
    void emitNewCodec();

  signals:
    void addCodec(QString alias);
};

/**
 *  @author Jérémy Quentin <jeremy.quentin@gmail.com>
 *
 *  \note see ticket #1309 for advices about how to improve this class.
 */
class DlgAccounts : public QWidget, public Ui_DlgAccountsBase
{
Q_OBJECT
public:
   DlgAccounts(KConfigDialog *parent = 0);

   void saveAccount(QListWidgetItem * item);

   /**
    *   Fills the settings form in the right side with the
    *   settings of @p item.
    *
    *   \note When the user creates a new account, its accountDetails
    *   map is empty, so the form is filled with blank strings,
    *   zeros... And when the user clicks \e Apply , these settings are
    *   saved just after the account is created. So be careful the form
    *   is filled with the right default settings if blank (as 600 for
    *   registration expire).
    *
    * @param item the item with which to fill the settings form
    *
    */
   void loadAccount(QListWidgetItem * item);

private:
   ///Attributes
   ConfigAccountList*  accountList           ;
   QList<StringHash>   codecList             ;
   QListWidgetItemHash credentialInfo        ;
   CredentialList      credentialList        ;
   bool                accountListHasChanged ;

   ///Mutators
   void loadCodecList();

public slots:
   void saveAccountList ();
   void loadAccountList ();
   bool hasChanged      ();
   void updateSettings  ();
   void updateWidgets   ();

private slots:
   void changedAccountList              ();
   void connectAccountsChangedSignal    ();
   void disconnectAccountsChangedSignal ();
   void on_button_accountUp_clicked     ();
   void on_button_accountDown_clicked   ();
   void on_button_accountAdd_clicked    ();
   void on_button_accountRemove_clicked ();
   void codecChanged                    ();
   void addCredential                   ();
   void removeCredential                ();
   void enablePublished                 ();
   void updateAccountStates             ();
   void updateAccountListCommands       ();

   void codecClicked                                 ( const QModelIndex& model                                 );
   void updateStatusLabel                            ( QListWidgetItem* item                                    );
   void on_listWidget_accountList_currentItemChanged ( QListWidgetItem* current , QListWidgetItem * previous    );
   void selectCredential                             ( QListWidgetItem* item    , QListWidgetItem* previous     );
   void addAccountToAccountList                      ( AccountView*   account                                   );
   void updateStatusLabel                            ( AccountView*   account                                   );
   void addCodec                                     ( QString        name = ""                                 );
   void updateCombo                                  ( int            value                                     );
   void loadCredentails                              ( QString        accountId                                 );
   void saveCredential                               ( QString        accountId                                 );
   void on_edit1_alias_textChanged                   ( const QString& text                                      );


signals:
   void updateButtons();

};

#endif
