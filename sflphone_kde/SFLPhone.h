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
	static QString firstAccount();

private slots:
	void typeChar(QChar c);
	void action(QListWidgetItem * item, call_action action);
	
	void updateWindowCallState();
	void updateRecordButton();
	void updateVolumeButton();
	void updateRecordBar();
	void updateVolumeBar();
	void updateVolumeControls();
	void updateDialpad();
	
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
	
	void on_slider_recVol_valueChanged(int value);
	void on_slider_sndVol_valueChanged(int value);
	
	void on_toolButton_recVol_clicked();
	void on_toolButton_sndVol_clicked();
	
	void on_listWidget_callList_currentItemChanged();

	void on_callStateChanged(const QString &callID, const QString &state);
	void on_error(MapStringString details);
	void on_incomingCall(const QString &accountID, const QString &callID, const QString &from);
	void on_incomingMessage(const QString &accountID, const QString &message);
	void on_voiceMailNotify(const QString &accountID, int count);
	void on_volumeChanged(const QString &device, double value);

};

#endif
 
