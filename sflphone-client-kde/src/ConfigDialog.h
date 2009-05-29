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
 
#ifndef HEADER_CONFIGDIALOG
#define HEADER_CONFIGDIALOG

#include <QtGui/QTableWidgetItem>
#include <QtGui/QListWidgetItem>
#include <QtCore/QString>
#include <QtGui/QAbstractButton>
#include <QErrorMessage>

#include "ui_ConfigDialog.h"
#include "AccountList.h"
#include "sflphone_kdeview.h"

class sflphone_kdeView;

class ConfigurationDialog : public QDialog, private Ui::ConfigurationDialog
{
	Q_OBJECT

private:
	static AccountList * accountList;
	QErrorMessage * errorWindow;
	MapStringString * codecPayloads;
	bool accountsChangedEnableWarning;
	

public:

	//Constructors & Destructors
	ConfigurationDialog(sflphone_kdeView *parent = 0);
	~ConfigurationDialog();
	
	//Getters
	static AccountList * getAccountList();

	//Setters
	void setPage(int page);
	void addAccountToAccountList(Account * account);
	
	void loadAccount(QListWidgetItem * item);
	void saveAccount(QListWidgetItem * item);

	void loadAccountList();
	void saveAccountList();
	
	void loadCodecs();
	void saveCodecs();

	void loadOptions();
	void saveOptions();
	
	//Updates
	void updateCodecListCommands();
	void updateAccountListCommands();
	void updateAccountStates();

private slots:
	void changedAccountList();
	
	void on_toolButton_codecUp_clicked();
	void on_toolButton_codecDown_clicked();
	void on_button_accountUp_clicked();
	void on_button_accountDown_clicked();
	void on_button_accountAdd_clicked();
	void on_button_accountRemove_clicked();
	void on_edit1_alias_textChanged(const QString & text);
	void on_listWidget_accountList_currentItemChanged ( QListWidgetItem * current, QListWidgetItem * previous );
	void on_spinBox_SIPPort_valueChanged ( int value );
	void on_buttonBoxDialog_clicked(QAbstractButton * button);
	void on_tableWidget_codecs_currentCellChanged(int currentRow);
	void on_toolButton_accountsApply_clicked();
	
	void on1_accountsChanged();
	void on1_parametersChanged();
	void on1_errorAlert(int code);
	
};

#endif 
