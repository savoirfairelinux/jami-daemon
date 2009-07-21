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
#ifndef DLGAUDIO_H
#define DLGAUDIO_H

#include <QWidget>
#include <kconfigdialog.h>

#include "ui_dlgaudiobase.h"
#include "conf/ConfigurationSkeleton.h"

/**
	@author Jérémy Quentin <jeremy.quentin@gmail.com>
*/
class DlgAudio : public QWidget, public Ui_DlgAudioBase
{
Q_OBJECT
public:
    DlgAudio(KConfigDialog *parent = 0);

    ~DlgAudio();

private:
	bool codecTableHasChanged;

public slots:
	void updateWidgets();
	void updateSettings();
	bool hasChanged();
	/**
	 *   Loads the ALSA settings to fill the combo boxes
	 *   of the ALSA settings.
	 *   ALSA choices for input, output... can be load only
	 *   when the daemon has set ALSA as sound manager.
	 *   So we have to load these settings once the user choses
	 *   ALSA.
	 */
	void loadAlsaSettings();
	
private slots:
	void codecTableChanged();
	
signals:
	void updateButtons();
};

#endif
