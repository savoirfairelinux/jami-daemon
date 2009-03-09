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

private slots:
	void on_buttonSupprimerCompte_clicked();
	void on_buttonNouveauCompte_clicked();
	void on_edit1_Alias_textChanged(const QString & text);
	void on_listWidgetComptes_currentItemChanged ( QListWidgetItem * current, QListWidgetItem * previous );
	void on_spinBox_PortSIP_valueChanged ( int value );
	void on_buttonBoxDialog_clicked(QAbstractButton * button);
};



#endif 
