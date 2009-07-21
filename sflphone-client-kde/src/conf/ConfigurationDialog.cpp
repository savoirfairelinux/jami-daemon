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

ConfigurationDialog::ConfigurationDialog(SFLPhoneView *parent)
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
	
	addPage( dlgGeneral      , i18nc("Config section", "General")      , "sflphone-client-kde" ); 
	addPage( dlgDisplay      , i18nc("Config section", "Display")      , "applications-graphics" ); 
	addPage( dlgAccounts     , i18nc("Config section", "Accounts")     , "personal" ); 
	addPage( dlgAudio        , i18nc("Config section", "Audio")        , "voicecall" ); 
	addPage( dlgAddressBook  , i18nc("Config section", "Address Book") , "x-office-address-book" ); 
	addPage( dlgRecord       , i18nc("Config section", "Recordings")       , "media-record" ); 
	addPage( dlgHooks        , i18nc("Config section", "Hooks")        , "insert-link" );
	
	connect(this, SIGNAL(applyClicked()), this,     SLOT(applyCustomSettings()));
	connect(this, SIGNAL(okClicked()),    this,     SLOT(applyCustomSettings()));
	
	connect(dlgGeneral, SIGNAL(clearCallHistoryAsked()), this, SIGNAL(clearCallHistoryAsked()));
}


ConfigurationDialog::~ConfigurationDialog()
{
}

void ConfigurationDialog::updateWidgets()
{
	qDebug() << "updateWidgets";
	dlgAudio->updateWidgets();
	dlgAccounts->updateWidgets();
}

void ConfigurationDialog::updateSettings()
{
	qDebug() << "updateSettings";
	dlgAudio->updateSettings();
	dlgAccounts->updateSettings();
	qDebug() << "alsaPlugin = " << ConfigurationSkeleton::self()->alsaPlugin();
}

bool ConfigurationDialog::hasChanged()
{
	qDebug() << "hasChanged" << dlgAudio->hasChanged() << dlgAccounts->hasChanged();
	return dlgAudio->hasChanged() || dlgAccounts->hasChanged();
}

void ConfigurationDialog::updateButtons()
{
	qDebug() << "updateButtons";
	enableButtonApply( hasChanged() );
}

void ConfigurationDialog::applyCustomSettings()
{
	qDebug() << "applyCustomSettings";
// 	if(hasChanged())
// 	{
		ConfigurationSkeleton::self()->writeConfig();
// 	}
	updateSettings();
	updateWidgets();
	updateButtons();
	emit changesApplied();
}

void ConfigurationDialog::reload()
{
	qDebug() << "reload";
	ConfigurationSkeleton::self()->readConfig();
	updateWidgets();
	updateButtons();
}
