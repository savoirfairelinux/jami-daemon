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

#ifndef sflphone_kdeVIEW_H
#define sflphone_kdeVIEW_H

#include <QtGui/QWidget>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QList>
#include <QtGui/QListWidgetItem>
#include <QtGui/QKeyEvent>
#include <QErrorMessage>
#include <KXmlGuiWindow>

#include "ui_sflphone_kdeview_base.h"
#include "ConfigDialog.h"
#include "CallList.h"
#include "AccountWizard.h"
#include "Contact.h"
#include "sflphone_kdeview.h"

#include "ui_sflphone_kdeview_base.h"

class ConfigurationDialog;
/**
 * This is the main view class for sflphone-client-kde.  Most of the non-menu,
 * non-toolbar, and non-statusbar (e.g., non frame) GUI code should go
 * here.
 *
 * @short Main view
 * @author Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>
 * @version 0.1
 */
 

class sflphone_kdeView : public QWidget, public Ui::SFLPhone_view
{
    Q_OBJECT
    
private:

	static ConfigurationDialog * configDialog;
	AccountWizard * wizard;
	CallList * callList;
	QErrorMessage * errorWindow;

protected:
	
	/**
	 * override context menu handling
	 * @param event
	 */
	void contextMenuEvent(QContextMenuEvent *event);

public:

	//Constructors & Destructors
	sflphone_kdeView(QWidget *parent);
	virtual ~sflphone_kdeView();
	/**
	 * 
	 */
	void loadWindow();
	void buildDialPad();
	
	//Getters
	static QString firstAccountId();
	static Account * firstRegisteredAccount();
	static QVector<Account *> registeredAccounts();
	static AccountList * getAccountList();
	QErrorMessage * getErrorWindow();
	
	//Daemon getters
	int phoneNumberTypesDisplayed();
	
	//Updates
	QVector<Contact *> findContactsInKAddressBook(QString textSearched, bool & full);
	
private slots:
	void actionb(Call * call, call_action action);
	void action(QListWidgetItem * item, call_action action);
	
	void setAccountFirst(Account * account);
	
	//void typeChar(QChar c);
	void typeString(QString str);
	void backspace();
	void escape();
	void enter();
	void editBeforeCall();
	
	void alternateColors(QListWidget * listWidget);
	
	void addCallToCallList(Call * call);
	void addCallToCallHistory(Call * call);
	void addContactToContactList(Contact * contact);
	
	void updateCallItem(Call * call);
	void updateWindowCallState();
	void updateSearchHistory();
	void updateCallHistory();
	void updateAddressBook();
	void updateRecordButton();
	void updateVolumeButton();
	void updateRecordBar();
	void updateVolumeBar();
	void updateVolumeControls();
	void updateDialpad();
	
	
	virtual void keyPressEvent(QKeyEvent *event)
	{
		int key = event->key();
		if(key == Qt::Key_Escape)
			escape();
		else if(key == Qt::Key_Return || key == Qt::Key_Enter)
			enter();
		else if(key == Qt::Key_Backspace)
			backspace();
		else
		{
			QString text = event->text();
			if(! event->text().isEmpty())
			{
				typeString(text);
			}
		}
	}

	void on_action_displayVolumeControls_triggered();
	void on_action_displayDialpad_triggered();
	void on_action_configureAccounts_triggered();
	void on_action_configureAudio_triggered();
	void on_action_configureSflPhone_triggered();
	void on_action_accountCreationWizard_triggered();
	void on_action_accept_triggered();
	void on_action_refuse_triggered();
	void on_action_hold_triggered();
	void on_action_transfer_triggered();
	void on_action_record_triggered();
	void on_action_history_triggered(bool checked);
	void on_action_addressBook_triggered(bool checked);
	void on_action_mailBox_triggered();
	
	void on_pushButton_1_clicked();
	void on_pushButton_2_clicked();
	void on_pushButton_3_clicked();
	void on_pushButton_4_clicked();
	void on_pushButton_5_clicked();
	void on_pushButton_6_clicked();
	void on_pushButton_7_clicked();
	void on_pushButton_8_clicked();
	void on_pushButton_9_clicked();
	void on_pushButton_0_clicked();
	void on_pushButton_diese_clicked();
	void on_pushButton_etoile_clicked();
	
	void on_lineEdit_searchHistory_textChanged();
	void on_lineEdit_addressBook_textChanged();
	
	void on_slider_recVol_valueChanged(int value);
	void on_slider_sndVol_valueChanged(int value);
	
	void on_toolButton_recVol_clicked(bool checked);
	void on_toolButton_sndVol_clicked(bool checked);
	
	void on_listWidget_callList_currentItemChanged();
	void on_listWidget_callList_itemChanged();
	void on_listWidget_callList_itemDoubleClicked(QListWidgetItem * item);
	void on_listWidget_callHistory_currentItemChanged();
	void on_listWidget_callHistory_itemDoubleClicked(QListWidgetItem * item);
	void on_listWidget_addressBook_currentItemChanged();
	void on_listWidget_addressBook_itemDoubleClicked(QListWidgetItem * item);
	
	void on_stackedWidget_screen_currentChanged(int index);
	
	void on1_callStateChanged(const QString &callID, const QString &state);
	void on1_error(MapStringString details);
	void on1_incomingCall(const QString &accountID, const QString &callID/*, const QString &from*/);
	void on1_incomingMessage(const QString &accountID, const QString &message);
	void on1_voiceMailNotify(const QString &accountID, int count);
	void on1_volumeChanged(const QString &device, double value);
	

};

#endif // sflphone_kdeVIEW_H
