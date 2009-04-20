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



SFLPhone::SFLPhone(QWidget *parent)
    : KXmlGuiWindow(parent),
      view(new sflphone_kdeView(this))
{
	// accept dnd
		setAcceptDrops(true);

    // tell the KXmlGuiWindow that this is indeed the main widget
		setCentralWidget(view);

    // then, setup our actions
   

    // add a status bar
//    statusBar()->show();

    // a call to KXmlGuiWindow::setupGUI() populates the GUI
    // with actions, using KXMLGUI.
    // It also applies the saved mainwindow settings, if any, and ask the
    // mainwindow to automatically save settings if changed: window size,
    // toolbar position, icon size, etc.
 
		setupActions();
		createGUI("/home/jquentin/sflphone/sflphone_kde/sflphone_kdeui.rc");
		setWindowIcon(QIcon(ICON_SFLPHONE));
} 

SFLPhone::~SFLPhone()
{
}

void SFLPhone::setupActions()
{
	qDebug() << "setupActions";
// 	KStandardAction::openNew(this, SLOT(fileNew()), actionCollection());
// 	KStandardAction::quit(qApp, SLOT(quit()), actionCollection());

// 	KStandardAction::preferences(this, SLOT(optionsPreferences()), actionCollection());

//     custom menu and menu item - the slot is in the class testkde4appfwView
// 	KAction *custom = new KAction(KIcon("colorize"), i18n("Swi&tch Colors"), this);
// 	actionCollection()->addAction( QLatin1String("switch_action"), custom );
// 	connect(custom, SIGNAL(triggered(bool)), view, SLOT(switchColors()));
// 	KAction * action_quit = KStandardAction::quit(qApp, SLOT(closeAllWindows()), menu_Actions);
// 	menu_Actions->addAction(action_quit);
	
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
	
	//KMenu * menu_Actions = new KMenu(tr2i18n("&Actions"));
	//actionCollection()->addMenu("Actions", menu_Actions);
	//menu_Actions->setObjectName(QString::fromUtf8("menu_Actions"));
// 	menu_Actions->addAction(view->action_accept);
// 	menu_Actions->addAction(view->action_refuse);
// 	menu_Actions->addAction(view->action_hold);
// 	menu_Actions->addAction(view->action_transfer);
// 	menu_Actions->addAction(view->action_record);
// 	menu_Actions->addSeparator();
// 	menu_Actions->addAction(view->action_history);
// 	menu_Actions->addAction(view->action_addressBook);
// 	menu_Actions->addSeparator();
// 	menu_Actions->addAction(view->action_mailBox);
// 	menu_Actions->addSeparator();
// 	KAction * action_quit = KStandardAction::quit(qApp, SLOT(closeAllWindows()), 0);
// 	menu_Actions->addAction(action_quit);
// 	qDebug() << "menuBar()->addMenu(menu_Actions) : " << menuBar()->addMenu(menu_Actions);
	//menuBar()->addMenu(menu_Actions);
	
	actionCollection()->addAction("action_displayVolumeControls", view->action_displayVolumeControls);
	actionCollection()->addAction("action_displayDialpad", view->action_displayDialpad);
	actionCollection()->addAction("action_configureAccounts", view->action_configureAccounts);
	actionCollection()->addAction("action_configureAudio", view->action_configureAudio);
	actionCollection()->addAction("action_configureSflPhone", view->action_configureSflPhone);
	actionCollection()->addAction("action_accountCreationWizard", view->action_accountCreationWizard);
	
// 	KMenu * menu_Configure = new KMenu(tr2i18n("&Settings"));
// 	menu_Configure->setObjectName(QString::fromUtf8("menu_Configure"));
// 	menu_Configure->addAction(view->action_displayVolumeControls);
// 	menu_Configure->addAction(view->action_displayDialpad);
// 	menu_Configure->addSeparator();
// 	menu_Configure->addAction(view->action_configureAccounts);
// 	menu_Configure->addAction(view->action_configureAudio);
// 	menu_Configure->addAction(view->action_configureSflPhone);
// 	menu_Configure->addSeparator();
// 	menu_Configure->addAction(view->action_accountCreationWizard);
// 	menuBar()->addMenu(menu_Configure);
	
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
	
	
}


bool SFLPhone::queryClose()
{
	qDebug() << "queryClose : " << view->listWidget_callList->count() << " calls open.";
	if(view->listWidget_callList->count() > 0)
	{
		qDebug() << "Attempting to quit when still having some calls open.";
		view->getErrorWindow()->showMessage(tr2i18n("You still have some calls open. Please close all calls before quitting.", 0));
		return false;
	}
	return true;
}



