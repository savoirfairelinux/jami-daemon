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
#include "ConfigurationDialog.h"

#include "conf/ConfigurationSkeleton.h"

#include "dlggeneral.h"
#include "dlgdisplay.h"
#include "dlgaccounts.h"
#include "dlgaudio.h"
#include "dlgaddressbook.h"
#include "dlgrecord.h"
#include "dlghooks.h"

#include "sflphone_const.h"

ConfigurationDialogKDE::ConfigurationDialogKDE(sflphone_kdeView *parent)
 :KConfigDialog(parent, SETTINGS_NAME, ConfigurationSkeleton::self())
{
	this->setWindowIcon(QIcon(ICON_SFLPHONE));
	
	dlgGeneral     = new DlgGeneral(this);
	dlgDisplay     = new DlgDisplay(this);
	dlgAccounts    = new DlgAccounts(this);
	dlgAudio       = new DlgAudio(this);
	dlgAddressBook = new DlgAddressBook(this);
	dlgRecord      = new DlgRecord(this);
	dlgHooks       = new DlgHooks(this);
	
	addPage( dlgGeneral      , i18n("General")      , "sflphone-client-kde" ); 
	addPage( dlgDisplay      , i18n("Display")      , "applications-graphics" ); 
	addPage( dlgAccounts     , i18n("Accounts")     , "personal" ); 
	addPage( dlgAudio        , i18n("Audio")        , "voicecall" ); 
	addPage( dlgAddressBook  , i18n("Address Book") , "x-office-address-book" ); 
	addPage( dlgRecord       , i18n("Record")       , "media-record" ); 
	addPage( dlgHooks        , i18n("Hooks")        , "insert-link" ); 
	connect(this, SIGNAL(applyClicked()), dlgAudio, SLOT(updateAlsaSettings()));
	connect(this, SIGNAL(okClicked()),    dlgAudio, SLOT(updateAlsaSettings()));
	connect(this, SIGNAL(applyClicked()), this,     SLOT(applyCustomSettings()));
	connect(this, SIGNAL(okClicked()),    this,     SLOT(applyCustomSettings()));
	
	connect(dlgGeneral, SIGNAL(clearCallHistoryAsked()), this, SIGNAL(clearCallHistoryAsked()));
// 	connect(this, SIGNAL(settingsChanged(const QString&)), this, SLOT(slot()));
// 	connect(this, SIGNAL(widgetModified()), this, SLOT(slot()));
}


ConfigurationDialogKDE::~ConfigurationDialogKDE()
{
}

void ConfigurationDialogKDE::slot()
{
	qDebug() << "slot";
}

void ConfigurationDialogKDE::updateWidgets()
{
	qDebug() << "updateWidgets";
	dlgAudio->updateWidgets();
	dlgAccounts->updateWidgets();
}

void ConfigurationDialogKDE::updateSettings()
{
	qDebug() << "updateSettings";
	dlgAudio->updateSettings();
	dlgAccounts->updateSettings();
	qDebug() << "yo  " << ConfigurationSkeleton::self()->alsaPlugin();
}

bool ConfigurationDialogKDE::hasChanged()
{
	qDebug() << "hasChanged";
	return dlgAudio->hasChanged() || dlgAccounts->hasChanged();
}

void ConfigurationDialogKDE::updateButtons()
{
	qDebug() << "updateButtons";
	enableButtonApply( hasChanged() );
}

void ConfigurationDialogKDE::applyCustomSettings()
{
	qDebug() << "applyCustomSettings";
	dlgAccounts->applyCustomSettings();
// 	if(hasChanged())
// 	{
		ConfigurationSkeleton::self()->writeConfig();
// 	}
	updateButtons();
}

void ConfigurationDialogKDE::reload()
{
	qDebug() << "reload";
	ConfigurationSkeleton::self()->readConfig();
	updateWidgets();
}
