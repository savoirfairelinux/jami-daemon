#ifndef SFLPHONE_H
#define SFLPHONE_H

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QListWidgetItem>
#include <QtGui/QKeyEvent>
#include <QErrorMessage>
#include <QSystemTrayIcon>

#include <KXmlGuiWindow>

#include "ui_sflphone_kdeview_base.h"
#include "ConfigDialog.h"
#include "CallList.h"
#include "AccountWizard.h"
#include "Contact.h"
#include "sflphone_kdeview.h"


class ConfigurationDialog;
class sflphone_kdeView;

class SFLPhone : public KXmlGuiWindow
{

Q_OBJECT

private:
	sflphone_kdeView * view;
	QMenu *trayIconMenu;
	bool iconChanged;
	QSystemTrayIcon *trayIcon;

protected:
	virtual bool queryClose();
	virtual void changeEvent(QEvent * event);

public:
	SFLPhone(QWidget *parent = 0);
	~SFLPhone();
	void setupActions();
	void sendNotif(QString caller);
	void putForeground();
	void trayIconSignal();
	
	
private slots:
	void on_trayIcon_activated(QSystemTrayIcon::ActivationReason reason);
	void on_trayIcon_messageClicked();


};

#endif
 
