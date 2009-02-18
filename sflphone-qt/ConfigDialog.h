#ifndef HEADER_CONFIGDIALOG
#define HEADER_CONFIGDIALOG

#include <QtGui>
#include "ui_ConfigDialog.h"
#include "daemon_interface_p.h"
#include "AccountList.h"

//struct QListViewItem;

class DaemonInterface;

class ConfigurationDialog : public QDialog, private Ui::ConfigurationDialog
{
	Q_OBJECT

public:
	ConfigurationDialog(QDialog *parent = 0);
	void loadAccount(QListWidgetItem * item);
	void saveAccount(QListWidgetItem * item);

private:
	DaemonInterface * daemon;
	AccountList * accountList;


    private slots:
        /* Insérez les prototypes de vos slots personnalisés ici */
	void on_buttonNouveauCompte_clicked();
	void on_edit1_Alias_textChanged(const QString & text);
	void on_listWidgetComptes_currentItemChanged ( QListWidgetItem * current, QListWidgetItem * previous );
};



#endif 
