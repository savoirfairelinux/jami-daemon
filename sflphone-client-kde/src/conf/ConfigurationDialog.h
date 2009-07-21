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
#ifndef CONFIGURATIONDIALOG_H
#define CONFIGURATIONDIALOG_H

#include <kconfigdialog.h>


#include "kcfg_settings.h"
#include "SFLPhoneView.h"


#define SETTINGS_NAME "settings"

class DlgGeneral;
class DlgDisplay;
class DlgAccounts;
class DlgAudio;
class DlgAddressBook;
class DlgRecord;
class DlgHooks;

class SFLPhoneView;

/**
	@author Jérémy Quentin <jeremy.quentin@gmail.com>
	This class represents the config dialog for sflphone.
	It uses the ConfigurationSkeleton class to handle most of the settings.
	It inherits KConfigDialog with the pages defined in dlg... files.
	A few complicated settings are handled directly by its pages.
	Some custom behaviors have been added to handle specific cases,
	as this config dialog is not the usual kind.
	A few things might be done a cleaner way by passing the handling 
	to the skeleton like it has been done with codecs.
*/
class ConfigurationDialog : public KConfigDialog
{
Q_OBJECT
private:

	
	DlgGeneral     * dlgGeneral;
	DlgDisplay     * dlgDisplay;
	DlgAccounts    * dlgAccounts;
	DlgAudio       * dlgAudio;
	DlgAddressBook * dlgAddressBook;
	DlgRecord      * dlgRecord;
	DlgHooks       * dlgHooks;

public:
	ConfigurationDialog(SFLPhoneView *parent = 0);

	~ConfigurationDialog();
	
    
public slots:
	/**
	 *   Reimplements KConfigDialog
	 */
	void updateWidgets();
	/**
	 *   Reimplements KConfigDialog
	 */
	void updateSettings();
	/**
	 *   Should be implemented in KConfigDialog but for no reason, is not.
	 *   For the moment it is here but has to be removed if implemented in KConfigDialog
	 *   because causes problems for a few cases (item managed by kconfig switched, item not managed
	 *   switched and then switched back, apply becomes disabled).
	 *   Can't be resolved without a method to know if items managed by kconfig have changed.
	 *   Disable/Enable Apply Button according to hasChanged() result
	 */
	void updateButtons();
	/**
	 * Same as updateButtons, should be implemented in KConfigDialog.
	 * @return whether any custom widget has changed in the dialog.
	 */
	bool hasChanged();
	
	/**
	 * reloads the informations before showing it.
	 */
	void reload();
	
private slots:
	/**
	 *   Apply settings not managed by kconfig (accounts)
	 *   Should be removed when accounts are managed by kconfig.
	 */
	void applyCustomSettings();


signals:
	void clearCallHistoryAsked();
	void changesApplied();
	
};

#endif
