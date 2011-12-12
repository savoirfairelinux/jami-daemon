/***************************************************************************
 *   Copyright (C) 2009-2010 by Savoir-Faire Linux                         *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
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
 **************************************************************************/

//Parent
#include "SFLPhoneTray.h"

//Qt
#include <QtCore/QDebug>
#include <QtGui/QMenu>
#include <QtGui/QIcon>

//KDE
#include <KAction>

///Constructor
SFLPhoneTray::SFLPhoneTray(QIcon icon, QWidget *parent)
      : KSystemTrayIcon(icon, parent),
         trayIconMenu(0),
         initialized_(false)
{
}

///Destructor
SFLPhoneTray::~SFLPhoneTray()
{
}

///Initializer
bool SFLPhoneTray::initialize()
{
   if ( initialized_ ) {
      qDebug() << "Already initialized.";
      return false;
   }

   trayIconMenu = new QMenu(parentWidget());
   setContextMenu(trayIconMenu);

   initialized_ = true;

   return true;
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Add a new action
void SFLPhoneTray::addAction(KAction *action)
{
   trayIconMenu->addAction(action);
}