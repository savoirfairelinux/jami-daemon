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


#include "settings.h"
#include "sflphone_kdeview.h"


#define SETTINGS_NAME "settings"

class DlgGeneral;
class DlgDisplay;
class DlgAccounts;
class DlgAudio;
class DlgAddressBook;
class DlgRecord;
class DlgHooks;

class sflphone_kdeView;

/**
	@author Jérémy Quentin <jeremy.quentin@gmail.com>
*/
class ConfigurationDialogKDE : public KConfigDialog
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
	ConfigurationDialogKDE(sflphone_kdeView *parent = 0);

	~ConfigurationDialogKDE();
	
    
public slots:
	void slot();
	void updateWidgets();
	void updateSettings();
	/**
	 *   Should be implemented in KConfigDialog but for no reason, is not.
	 *   For the moment it is here but has to be removed if implemented in KConfigDialog
	 *   because causes problems for a few cases (item managed by kconfig switched, item not managed
	 *   switched and then switched back, apply becomes disabled).
	 *   Can't be resolved without a method to know if items managed by kconfig have changed.
	 */
	void updateButtons();
	bool hasChanged();
	
	/**
	 * reloads the informations before showing it.
	 */
	void reload();
	
private slots:
	void applyCustomSettings();

};

#endif
