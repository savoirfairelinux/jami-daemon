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
 ***************************************************************************/
#ifndef DLGACCOUNTS_H
#define DLGACCOUNTS_H

#include <QWidget>
#include <kconfigdialog.h>
#include <QTableWidget>

#include "ui_dlgaccountsbase.h"
#include "Account.h"
#include "AccountList.h"
#include <QDebug>

typedef QHash<QString, QString> StringHash; //Needed to fix a Qt foreach macro argument parsing bug

class Private_AddCodecDialog : public KDialog {
  Q_OBJECT
  public:
    Private_AddCodecDialog(QList< StringHash > itemList, QStringList currentItems ,QWidget* parent = 0) : KDialog(parent) {
      codecTable = new QTableWidget(this);
      codecTable->verticalHeader()->setVisible(false);
      codecTable->setColumnCount(5);
      codecTable->setSelectionBehavior(QAbstractItemView::SelectRows);
      int i =0;
      foreach (StringHash aCodec, itemList) {
        if ( currentItems.indexOf(aCodec["alias"]) == -1) {
          codecTable->setRowCount(i+1);
          QTableWidgetItem* cName = new  QTableWidgetItem(aCodec["name"]);
          codecTable->setItem(i,0,cName);
          QTableWidgetItem* cBitrate = new  QTableWidgetItem(aCodec["bitrate"]);
          codecTable->setItem(i,1,cBitrate);
          QTableWidgetItem* cFrequency = new  QTableWidgetItem(aCodec["frequency"]);
          codecTable->setItem(i,2,cFrequency);
          QTableWidgetItem* cBandwidth = new  QTableWidgetItem(aCodec["bandwidth"]);
          codecTable->setItem(i,3,cBandwidth);
          QTableWidgetItem* cAlias = new  QTableWidgetItem(aCodec["alias"]);
          codecTable->setItem(i,4,cAlias);
          i++;
        }
      }
      setMainWidget(codecTable);
      resize(400,300);
      
      connect(this, SIGNAL(okClicked()), this, SLOT(emitNewCodec()));
    }
  private:
    QTableWidget* codecTable;
  private slots:
    void emitNewCodec() {
      emit addCodec(codecTable->item(codecTable->currentRow(),4)->text());
    }
  signals:
    void addCodec(QString alias);
};

/**
   @author Jérémy Quentin <jeremy.quentin@gmail.com>
   
   \note see ticket #1309 for advices about how to improve this class.
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
    */
   void loadAccount(QListWidgetItem * item);
   
private:
   AccountList * accountList;
        QList< StringHash > codecList;
   bool accountListHasChanged;
        void loadCodecList();

public slots:
   void saveAccountList();
   void loadAccountList();
   
   bool hasChanged();
   void updateSettings();
   void updateWidgets();
   
private slots:
   void changedAccountList();
   void connectAccountsChangedSignal();
   void disconnectAccountsChangedSignal();
   void on_button_accountUp_clicked();
   void on_button_accountDown_clicked();
   void on_button_accountAdd_clicked();
   void on_button_accountRemove_clicked();
   void on_edit1_alias_textChanged(const QString & text);
   void on_listWidget_accountList_currentItemChanged ( QListWidgetItem * current, QListWidgetItem * previous );
//    //void on_toolButton_accountsApply_clicked(); //Disabled for future removal
   void updateAccountStates();
   void addAccountToAccountList(Account * account);
   void updateAccountListCommands();
   void updateStatusLabel(QListWidgetItem * item);
   void updateStatusLabel(Account * account);
        void codecClicked(const QModelIndex & model);
        void addCodec(QString name = "");
        void codecChanged();
   
   
signals:
   void updateButtons();

};

#endif
