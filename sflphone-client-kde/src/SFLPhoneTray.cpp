/************************************** *************************************
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

#include <QDebug>

#include "SFLPhoneTray.h"

SFLPhoneTray::SFLPhoneTray(QIcon icon, QWidget *parent)
        : KSystemTrayIcon(icon, parent),
          initialized_(false),
          trayIconMenu(0)
{
}

SFLPhoneTray::~SFLPhoneTray()
{
}

bool SFLPhoneTray::initialize()
{
    if ( initialized_ )
    {
        qDebug() << "Already initialized.";
        return false;
    }

    trayIconMenu = new QMenu(parentWidget());
    setContextMenu(trayIconMenu);

    setupActions();

    initialized_ = true;

    return true;
}

void SFLPhoneTray::addAction(KAction *action)
{
    trayIconMenu->addAction(action);
}

void SFLPhoneTray::setupActions()
{
    qDebug() << "setupActions";
}

/*void SFLPhone::putForeground()
{
    activateWindow();
    hide();
    activateWindow();
    show();
    activateWindow();
}
*/

/*void SFLPhone::trayIconSignal()
{
    if(! parentWidget()->isActiveWindow())
    {
        setIcon(QIcon(ICON_TRAY_NOTIF));
        iconChanged = true;
    }
}*/

/*
void SFLPhone::on_trayIcon_activated(KSystemTrayIcon::ActivationReason reason)
{
    qDebug() << "on_trayIcon_activated";
    switch (reason) {
        case KSystemTrayIcon::Trigger:
        case KSystemTrayIcon::DoubleClick:
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
*/
