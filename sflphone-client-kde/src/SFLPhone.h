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
	QLabel * statusBarWidget;
	
private:
	void setObjectNames();

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
	void on_view_statusMessageChanged(const QString & message);

	void quitButton();

};

#endif
 
