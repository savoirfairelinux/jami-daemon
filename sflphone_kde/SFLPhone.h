#ifndef SFLPHONE_H
#define SFLPHONE_H

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QListWidgetItem>
#include <QtGui/QKeyEvent>
#include <QErrorMessage>
#include <KXmlGuiWindow>

#include "ui_sflphone-qt.h"
#include "ConfigDialog.h"
#include "CallList.h"
#include "AccountWizard.h"
#include "Contact.h"

class ConfigurationDialog;

class SFLPhone : public KXmlGuiWindow, private Ui::SFLPhone
{

Q_OBJECT

private:
	static ConfigurationDialog * configDialog;
	AccountWizard * wizard;
	CallList * callList;
	QErrorMessage * errorWindow;

protected:
	void contextMenuEvent(QContextMenuEvent *event);
	virtual bool queryClose();

public:
	SFLPhone(QMainWindow *parent = 0);
	~SFLPhone();
	void loadWindow();
	static QString firstAccountId();
	static Account * firstRegisteredAccount();
	static QVector<Account *> registeredAccounts();
	static AccountList * getAccountList();
	QVector<Contact *> findContactsInKAddressBook(QString textSearched);
	bool phoneNumberTypeDisplayed(int type);

private slots:
	//void typeChar(QChar c);
	void typeString(QString str);
	void backspace();
	void actionb(Call * call, call_action action);
	void action(QListWidgetItem * item, call_action action);
	void setupActions();
	
	void addCallToCallList(Call * call);
	void addCallToCallHistory(Call * call);
	void addContactToContactList(Contact * contact);
	
	void updateCallItem(Call * call);
	void updateWindowCallState();
	void updateSearchHistory();
	void updateSearchAddressBook();
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
		on_action_refuse_triggered();
	else if(key == Qt::Key_Return || key == Qt::Key_Enter)
		on_action_accept_triggered();
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

	
	void on_action_displayVolumeControls_toggled();
	void on_action_displayDialpad_toggled();
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
	//void on_actionAbout();
	
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
	void on_listWidget_addressBook_currentItemChanged();

	void on1_callStateChanged(const QString &callID, const QString &state);
	void on1_error(MapStringString details);
	void on1_incomingCall(const QString &accountID, const QString &callID, const QString &from);
	void on1_incomingMessage(const QString &accountID, const QString &message);
	void on1_voiceMailNotify(const QString &accountID, int count);
	void on1_volumeChanged(const QString &device, double value);
	
	void setAccountFirst(Account * account);

};

#endif
 
