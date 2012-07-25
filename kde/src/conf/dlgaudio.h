/****************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                               *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#ifndef DLGAUDIO_H
#define DLGAUDIO_H

#include <QWidget>
#include <kconfigdialog.h>

#include "ui_dlgaudiobase.h"
#include "klib/ConfigurationSkeleton.h"

/**
   @author Jérémy Quentin <jeremy.quentin@gmail.com>
*/
class DlgAudio : public QWidget, public Ui_DlgAudioBase
{
Q_OBJECT
public:
   //Constructor
   DlgAudio(KConfigDialog *parent = 0);

   //Destructor
   ~DlgAudio();

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
   //void codecTableChanged();

signals:
   ///Emitted when the buttons need to be updated in the parent dialog
   void updateButtons();
};

#endif
