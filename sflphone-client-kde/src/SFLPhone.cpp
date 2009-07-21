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
 
#include "SFLPhone.h"

#include <KApplication>
#include <KStandardAction>
#include <KMenuBar>
#include <KMenu>
#include <KAction>
#include <KToolBar>
#include <KStatusBar>
#include <QtGui/QStatusBar>
#include <QtGui/QCursor>
#include <KActionCollection>

#include "sflphone_const.h"
#include "instance_interface_singleton.h"
#include "configurationmanager_interface_singleton.h"


SFLPhone::SFLPhone(QWidget *parent)
    : KXmlGuiWindow(parent),
      view(new SFLPhoneView(this))
{
	
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	// accept dnd
		setAcceptDrops(true);

    // tell the KXmlGuiWindow that this is indeed the main widget
		setCentralWidget(view);

		setWindowIcon(QIcon(ICON_SFLPHONE));
		setWindowTitle(i18n("SFLphone"));
		
		setupActions();
		
		setObjectNames();
		QMetaObject::connectSlotsByName(this);
	   view->on_stackedWidget_screen_currentChanged(SCREEN_MAIN);
	   view->loadWindow();
	   
	   
		move(QCursor::pos().x() - geometry().width()/2, QCursor::pos().y() - geometry().height()/2);
	   if( ! configurationManager.isStartHidden())
	   {
	   	show();
	   }
	   
	   if(configurationManager.getAccountList().value().isEmpty())
		{
			(new AccountWizard())->show();
		}   
} 

SFLPhone::~SFLPhone()
{
}

void SFLPhone::setObjectNames()
{
	view->setObjectName("view");
	statusBar()->setObjectName("statusBar");
	trayIcon->setObjectName("trayIcon");
}

void SFLPhone::setupActions()
{
	qDebug() << "setupActions";
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	
	action_accept = new KAction(this);
	action_refuse = new KAction(this);
	action_hold = new KAction(this);
	action_transfer = new KAction(this);
	action_record = new KAction(this);
	action_mailBox = new KAction(this);
	
	action_screen = new QActionGroup(this);
	action_screen->setExclusive(true);
	action_main = new KAction(KIcon(QIcon(ICON_SCREEN_MAIN)), i18n("Main screen"), action_screen);
	action_history = new KAction(KIcon(QIcon(ICON_SCREEN_HISTORY)), i18n("Call history"), action_screen);
	action_addressBook = new KAction(KIcon(QIcon(ICON_SCREEN_ADDRESS)), i18n("Address book"), action_screen);
	action_main->setCheckable(true);
	action_history->setCheckable(true);
	action_addressBook->setCheckable(true);
	action_main->setChecked(true);
	action_screen->addAction(action_main);
	action_screen->addAction(action_history);
	action_screen->addAction(action_addressBook);
	
	action_close = KStandardAction::close(this, SLOT(close()), this);
	action_quit = KStandardAction::quit(this, SLOT(quitButton()), this);
	
	action_configureSflPhone = KStandardAction::preferences(view, SLOT(configureSflPhone()), this);
	action_configureSflPhone->setText(i18n("Configure SFLphone"));
	
	action_displayVolumeControls = new KAction(KIcon(QIcon(ICON_DISPLAY_VOLUME_CONSTROLS)), i18n("Display volume controls"), this);
	action_displayDialpad = new KAction(KIcon(QIcon(ICON_DISPLAY_DIALPAD)), i18n("Display dialpad"), this);
	action_displayVolumeControls->setChecked(configurationManager.getVolumeControls());
	action_displayDialpad->setChecked(configurationManager.getDialpad());
	action_accountCreationWizard = new KAction(i18n("Account creation wizard"), this);
	
	connect(action_accept,                SIGNAL(triggered()),          view, SLOT(accept()));
	connect(action_refuse,                SIGNAL(triggered()),          view, SLOT(refuse()));
	connect(action_hold,                  SIGNAL(triggered()),          view, SLOT(hold()));
	connect(action_transfer,              SIGNAL(triggered()),          view, SLOT(transfer()));
	connect(action_record,                SIGNAL(triggered()),          view, SLOT(record()));
	connect(action_screen,                SIGNAL(triggered(QAction *)), this, SLOT(updateScreen(QAction *)));
	connect(action_mailBox,               SIGNAL(triggered()),          view, SLOT(mailBox()));
	connect(action_displayVolumeControls, SIGNAL(triggered()),          view, SLOT(displayVolumeControls()));
	connect(action_displayDialpad,        SIGNAL(triggered()),          view, SLOT(displayDialpad()));
	connect(action_accountCreationWizard, SIGNAL(triggered()),          view, SLOT(accountCreationWizard()));
	
	action_screen->addAction(action_main);
	action_screen->addAction(action_history);
	action_screen->addAction(action_addressBook);
	
	actionCollection()->addAction("action_accept", action_accept);
	actionCollection()->addAction("action_refuse", action_refuse);
	actionCollection()->addAction("action_hold", action_hold);
	actionCollection()->addAction("action_transfer", action_transfer);
	actionCollection()->addAction("action_record", action_record);
	actionCollection()->addAction("action_main", action_main);
	actionCollection()->addAction("action_history", action_history);
	actionCollection()->addAction("action_addressBook", action_addressBook);
	actionCollection()->addAction("action_mailBox", action_mailBox);
	actionCollection()->addAction("action_close", action_close);
	actionCollection()->addAction("action_quit", action_quit);
	
	actionCollection()->addAction("action_displayVolumeControls", action_displayVolumeControls);
	actionCollection()->addAction("action_displayDialpad", action_displayDialpad);
	actionCollection()->addAction("action_configureSflPhone", action_configureSflPhone);
	actionCollection()->addAction("action_accountCreationWizard", action_accountCreationWizard);
	
	statusBarWidget = new QLabel();
	statusBar()->addWidget(statusBarWidget);
	
 	trayIconMenu = new QMenu(this);
 	trayIconMenu->addAction(action_quit);

	trayIcon = new QSystemTrayIcon(this->windowIcon(), this);
	trayIcon->setContextMenu(trayIconMenu);
	trayIcon->show();
	
	iconChanged = false;
	
	QString rcFilePath = QString(DATA_INSTALL_DIR) + "/sflphone-client-kde/sflphone-client-kdeui.rc";
	if(! QFile::exists(rcFilePath))
	{
		QDir dir;
		dir.cdUp();
		dir.cd("data");
		rcFilePath = dir.filePath("sflphone-client-kdeui.rc");
	}
	qDebug() << "rcFilePath = " << rcFilePath ;
	createGUI(rcFilePath);

}

SFLPhoneView * SFLPhone::getView()
{
	return view;
}

bool SFLPhone::queryClose()
{
	qDebug() << "queryClose";
	hide();
	return false;
}

void SFLPhone::quitButton()
{
	InstanceInterface & instance = InstanceInterfaceSingleton::getInstance();
	qDebug() << "quitButton : " << view->listWidget_callList->count() << " calls open.";
	if(view->listWidget_callList->count() > 0 && instance.getRegistrationCount() <= 1)
	{
		qDebug() << "Attempting to quit when still having some calls open.";
// 		view->getErrorWindow()->showMessage(i18n("You still have some calls open. Please close all calls before quitting."));
	}
	instance.Unregister(getpid());
	qApp->quit();
}


void SFLPhone::putForeground()
{
	activateWindow();
	hide();
	activateWindow();
	show();
	activateWindow();
}

void SFLPhone::trayIconSignal()
{
	if(! isActiveWindow())
	{
		trayIcon->setIcon(QIcon(ICON_TRAY_NOTIF));
		iconChanged = true;
	}
}

void SFLPhone::sendNotif(QString caller)
{
	trayIcon->showMessage(
	    i18n("Incoming call"), 
	    i18n("You have an incoming call from") + " " + caller + ".\n" + i18n("Click to accept or refuse it."), 
	    QSystemTrayIcon::Warning, 
	    20000);
}

void SFLPhone::on_trayIcon_messageClicked()
{
	qDebug() << "on_trayIcon_messageClicked";
	putForeground();
}

void SFLPhone::changeEvent(QEvent * event)
{
	if (event->type() == QEvent::ActivationChange && iconChanged && isActiveWindow())
	{
		trayIcon->setIcon(this->windowIcon());
		iconChanged = false;
	}
}

void SFLPhone::on_trayIcon_activated(QSystemTrayIcon::ActivationReason reason)
{
	qDebug() << "on_trayIcon_activated";
	switch (reason) {
		case QSystemTrayIcon::Trigger:
		case QSystemTrayIcon::DoubleClick:
			qDebug() << "Tray icon clicked.";
			if(isActiveWindow())
			{
				qDebug() << "isactive -> hide()";
				hide();
			}
			else
			{
				qDebug() << "isnotactive -> show()";
				putForeground();
			}
			break;
		default:
			qDebug() << "Tray icon activated with unknown reason.";
			break;
	}
}


void SFLPhone::on_view_statusMessageChangeAsked(const QString & message)
{
	qDebug() << "on_view_statusMessageChangeAsked : " + message;
	statusBarWidget->setText(message);
}

void SFLPhone::on_view_windowTitleChangeAsked(const QString & message)
{
	qDebug() << "on_view_windowTitleChangeAsked : " + message;
	setWindowTitle(message);
}

void SFLPhone::on_view_enabledActionsChangeAsked(const bool * enabledActions)
{
	qDebug() << "on_view_enabledActionsChangeAsked";
	action_accept->setEnabled(enabledActions[SFLPhone::Accept]);
	action_refuse->setEnabled(enabledActions[SFLPhone::Refuse]);
	action_hold->setEnabled(enabledActions[SFLPhone::Hold]);
	action_transfer->setEnabled(enabledActions[SFLPhone::Transfer]);
	action_record->setEnabled(enabledActions[SFLPhone::Record]);
	action_mailBox->setEnabled(enabledActions[SFLPhone::Mailbox]);
}

void SFLPhone::on_view_actionIconsChangeAsked(const QString * actionIcons)
{
	qDebug() << "on_view_actionIconsChangeAsked";
	action_accept->setIcon(QIcon(actionIcons[SFLPhone::Accept]));
	action_refuse->setIcon(QIcon(actionIcons[SFLPhone::Refuse]));
	action_hold->setIcon(QIcon(actionIcons[SFLPhone::Hold]));
	action_transfer->setIcon(QIcon(actionIcons[SFLPhone::Transfer]));
	action_record->setIcon(QIcon(actionIcons[SFLPhone::Record]));
	action_mailBox->setIcon(QIcon(actionIcons[SFLPhone::Mailbox]));
}

void SFLPhone::on_view_actionTextsChangeAsked(const QString * actionTexts)
{
	qDebug() << "on_view_actionTextsChangeAsked";
	action_accept->setText(actionTexts[SFLPhone::Accept]);
	action_refuse->setText(actionTexts[SFLPhone::Refuse]);
	action_hold->setText(actionTexts[SFLPhone::Hold]);
	action_transfer->setText(actionTexts[SFLPhone::Transfer]);
	action_record->setText(actionTexts[SFLPhone::Record]);
	action_mailBox->setText(actionTexts[SFLPhone::Mailbox]);
}


void SFLPhone::on_view_transferCheckStateChangeAsked(bool transferCheckState)
{
	qDebug() << "Changing transfer action checkState";
	action_transfer->setChecked(transferCheckState);
}

void SFLPhone::on_view_recordCheckStateChangeAsked(bool recordCheckState)
{
	qDebug() << "Changing record action checkState";
	action_record->setChecked(recordCheckState);
}

void SFLPhone::updateScreen(QAction * action)
{
	if(action == action_main)	view->changeScreen(SCREEN_MAIN);
	else if(action == action_history)	view->changeScreen(SCREEN_HISTORY);
	else if(action == action_addressBook)	view->changeScreen(SCREEN_ADDRESS);
}

void SFLPhone::on_view_screenChanged(int screen)
{
	qDebug() << "on_view_screenChanged";
	if(screen == SCREEN_MAIN)	action_main->setChecked(true);
	else if(screen == SCREEN_HISTORY)	action_history->setChecked(true);
	else if(screen == SCREEN_ADDRESS)	action_addressBook->setChecked(true);
}

QList <QAction *> SFLPhone::getCallActions()
{
	QList<QAction *> callActions = QList<QAction *>();
	callActions.insert((int) Accept, action_accept);
	callActions.insert((int) Refuse, action_refuse);
	callActions.insert((int) Hold, action_hold);
	callActions.insert((int)Transfer, action_transfer);
	callActions.insert((int) Record, action_record);
	callActions.insert((int) Mailbox, action_mailBox);
	return callActions;
}

void SFLPhone::on_view_incomingCall(const Call * call)
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	trayIconSignal();
	if(configurationManager.popupMode())
	{
		putForeground();
	}
	if(configurationManager.getNotify())
	{
		sendNotif(call->getPeerName().isEmpty() ? call->getPeerPhoneNumber() : call->getPeerName());
	}
}

