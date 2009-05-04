#include "SFLPhone.h"

#include <KApplication>
#include <KStandardAction>
#include <KMenuBar>
#include <KMenu>
#include <KAction>
#include <KToolBar>
#include <QtGui/QStatusBar>
#include <KActionCollection>

#include "sflphone_const.h"
#include "instance_interface_singleton.h"



SFLPhone::SFLPhone(QWidget *parent)
    : KXmlGuiWindow(parent),
      view(new sflphone_kdeView(this))
{
	// accept dnd
		setAcceptDrops(true);

    // tell the KXmlGuiWindow that this is indeed the main widget
		setCentralWidget(view);
   

    // add a status bar
//    statusBar()->show();


		setWindowIcon(QIcon(ICON_SFLPHONE));
		
		setupActions();
		
		QString str = QString(DATA_INSTALL_DIR) + "/sflphone-client-kde/sflphone-client-kdeui.rc";
		qDebug() << "str = " << str ;
		createGUI(str);
		
		connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
             this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

} 

SFLPhone::~SFLPhone()
{
}

void SFLPhone::setupActions()
{
	qDebug() << "setupActions";
	
	actionCollection()->addAction("action_accept", view->action_accept);
	actionCollection()->addAction("action_refuse", view->action_refuse);
	actionCollection()->addAction("action_hold", view->action_hold);
	actionCollection()->addAction("action_transfer", view->action_transfer);
	actionCollection()->addAction("action_record", view->action_record);
	actionCollection()->addAction("action_history", view->action_history);
	actionCollection()->addAction("action_addressBook", view->action_addressBook);
	actionCollection()->addAction("action_mailBox", view->action_mailBox);
	KAction * action_quit = KStandardAction::quit(qApp, SLOT(closeAllWindows()), 0);
	actionCollection()->addAction("action_quit", action_quit);
	
	
	actionCollection()->addAction("action_displayVolumeControls", view->action_displayVolumeControls);
	actionCollection()->addAction("action_displayDialpad", view->action_displayDialpad);
	actionCollection()->addAction("action_configureSflPhone", view->action_configureSflPhone);
	actionCollection()->addAction("action_configureAccounts", view->action_configureAccounts);
	actionCollection()->addAction("action_configureAudio", view->action_configureAudio);
	actionCollection()->addAction("action_accountCreationWizard", view->action_accountCreationWizard);
	
	
	QStatusBar * statusbar = new QStatusBar(this);
	statusbar->setObjectName(QString::fromUtf8("statusbar"));
	this->setStatusBar(statusbar);
	
	QToolBar * toolbar = new QToolBar(this);
	this->addToolBar(Qt::TopToolBarArea, toolbar);
	toolbar->addAction(view->action_accept);
	toolbar->addAction(view->action_refuse);
	toolbar->addAction(view->action_hold);
	toolbar->addAction(view->action_transfer);
	toolbar->addAction(view->action_record);
	toolbar->addSeparator();
	toolbar->addAction(view->action_history);
	toolbar->addAction(view->action_addressBook);
	toolbar->addSeparator();
	toolbar->addAction(view->action_mailBox);
	
	
 	trayIconMenu = new QMenu(this);
 	trayIconMenu->addAction(action_quit);

	trayIcon = new QSystemTrayIcon(this->windowIcon(), this);
	trayIcon->setContextMenu(trayIconMenu);
	trayIcon->show();
	qDebug() << "trayicon = " << trayIcon->icon();

}


bool SFLPhone::queryClose()
{
	InstanceInterface & instance = InstanceInterfaceSingleton::getInstance();
	qDebug() << "queryClose : " << view->listWidget_callList->count() << " calls open.";
	if(view->listWidget_callList->count() > 0 && instance.getRegistrationCount() <= 1)
	{
		qDebug() << "Attempting to quit when still having some calls open.";
		view->getErrorWindow()->showMessage(tr2i18n("You still have some calls open. Please close all calls before quitting.", 0));
		return false;
	}
	instance.Unregister(getpid());
	return true;
}


void SFLPhone::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
	qDebug() << "on_trayIcon_activated";
	switch (reason) {
		case QSystemTrayIcon::Trigger:
		case QSystemTrayIcon::DoubleClick:
			qDebug() << "Tray icon clicked.";
			qDebug() << "windowState() = " << windowState();
			if(isActiveWindow())
			{
				qDebug() << "isactive";
				hide();
			}
			else
			{
				qDebug() << "isnotactive";
				hide();
				show();
				activateWindow();
			}
			break;
// 		case QSystemTrayIcon::DoubleClick:
// 			iconComboBox->setCurrentIndex((iconComboBox->currentIndex() + 1)
// 													% iconComboBox->count());
// 			break;
// 		case QSystemTrayIcon::MiddleClick:
// 			showMessage();
// 			break;
		default:
			qDebug() << "Tray icon activated with unknown reason.";
			break;
	}
}



