#ifndef HEADER_CONFIGDIALOG
#define HEADER_CONFIGDIALOG

#include <QtGui>
#include "ui_ConfigDialog.h"
#include "configurationmanager_interface_p.h"
#include "AccountList.h"
#include "SFLPhone.h"
#include <QErrorMessage>

class SFLPhone;

class ConfigurationDialog : public QDialog, private Ui::ConfigurationDialog
{
	Q_OBJECT

private:
	AccountList * accountList;
	QErrorMessage * errorWindow;
	MapStringString * codecPayloads;

public:
	ConfigurationDialog(SFLPhone *parent = 0);
	~ConfigurationDialog();

	void loadAccount(QListWidgetItem * item);
	void saveAccount(QListWidgetItem * item);

	void loadAccountList();
	void saveAccountList();

	void loadCodecs();
	void saveCodecs();

	void loadOptions();
	void saveOptions();
	
	void setPage(int page);
	
	void updateCodecListCommands();
	void updateAccountListCommands();

private slots:
	void on_toolButton_codecUp_clicked();
	void on_toolButton_codecDown_clicked();
	void on_button_accountUp_clicked();
	void on_button_accountDown_clicked();
	void on_button_accountAdd_clicked();
	void on_button_accountRemove_clicked();
	void on_edit1_alias_textChanged(const QString & text);
	void on_listWidget_accountList_currentItemChanged ( QListWidgetItem * current, QListWidgetItem * previous );
	void on_listWidget_codecs_currentItemChanged ( QListWidgetItem * current, QListWidgetItem * previous );
	void on_spinBox_SIPPort_valueChanged ( int value );
	void on_buttonBoxDialog_clicked(QAbstractButton * button);
	void on_tableWidget_codecs_currentItemChanged(QTableWidgetItem * current, QTableWidgetItem * previous);
	void on_tableWidget_codecs_currentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn);

};

#endif 
