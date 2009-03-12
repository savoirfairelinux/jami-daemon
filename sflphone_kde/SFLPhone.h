#ifndef SFLPHONE_H
#define SFLPHONE_H

#include <QtGui>
#include "ui_sflphone-qt.h"
#include "ConfigDialog.h"
#include "CallList.h"

class ConfigurationDialog;

class SFLPhone : public QMainWindow, private Ui::SFLPhone
{

Q_OBJECT

private:
	ConfigurationDialog * configDialog;
	CallList * callList;

public:
	SFLPhone(QMainWindow *parent = 0);
	~SFLPhone();
	void loadWindow();
	QAbstractButton * getDialpadButton(int ind);
	static QString firstAccount();

private slots:
	void typeChar(QChar c);
	void action(QListWidgetItem * item, call_action action);
	void updateWindowCallState();

	void on_actionAfficher_les_barres_de_volume_toggled();
	void on_actionAfficher_le_clavier_toggled();
	void on_actionConfigurer_les_comptes_triggered();
	void on_actionConfigurer_le_son_triggered();
	void on_actionConfigurer_SFLPhone_triggered();
	void on_actionDecrocher_triggered();
	void on_actionRaccrocher_triggered();
	void on_actionMettre_en_attente_triggered();
	void on_actionTransferer_triggered();
	void on_actionRecord_triggered();
	void on_actionHistorique_triggered();
	void on_actionBoite_vocale_triggered();
	void on_actionAbout();

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
	
	void on_listWidget_callList_currentItemChanged();

};

#endif
 
