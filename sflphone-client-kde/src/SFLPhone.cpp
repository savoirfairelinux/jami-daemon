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
		setWindowTitle(tr2i18n("SFLPhone"));
		
		setupActions();
		
		qDebug() << "currentPath = " << QDir::currentPath() ;
		
		
		
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

      QMetaObject::connectSlotsByName(this);

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
	KAction * action_close = KStandardAction::close(this, SLOT(close()), 0);
	actionCollection()->addAction("action_close", action_close);
	KAction * action_quit = KStandardAction::quit(this, SLOT(quitButton()), 0);
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
	toolbar->setIconSize(QSize(TOOLBAR_SIZE,TOOLBAR_SIZE));
	
	
 	trayIconMenu = new QMenu(this);
 	trayIconMenu->addAction(action_quit);

	trayIcon = new QSystemTrayIcon(this->windowIcon(), this);
	trayIcon->setContextMenu(trayIconMenu);
	trayIcon->show();
	trayIcon->setObjectName("trayIcon");
	
	iconChanged = false;

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
		view->getErrorWindow()->showMessage(tr2i18n("You still have some calls open. Please close all calls before quitting.", 0));
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
	    tr2i18n("Incoming call"), 
	    tr2i18n("You have an incoming call from") + " " + caller + ".\n" + tr2i18n("Click to accept or refuse it."), 
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



