/***************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                              *
 *   Author : Jérémy Quentin                                               *
 *   jeremy.quentin@savoirfairelinux.com                                   *
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

#include "ui_dlgaccountsbase.h"
#include "Account.h"
#include "AccountList.h"

/**
	@author Jérémy Quentin <jeremy.quentin@gmail.com>
*/
class DlgAccounts : public QWidget, public Ui_DlgAccountsBase
{
Q_OBJECT
public:
	DlgAccounts(KConfigDialog *parent = 0);

	void saveAccount(QListWidgetItem * item);
	void loadAccount(QListWidgetItem * item);
	
private:
	AccountList * accountList;
	bool accountListHasChanged;

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
	void on_toolButton_accountsApply_clicked();
	void updateAccountStates();
	void addAccountToAccountList(Account * account);
	void updateAccountListCommands();
	
	
signals:
	void updateButtons();

};

#endif
