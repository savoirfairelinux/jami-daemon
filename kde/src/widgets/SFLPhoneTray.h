/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
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
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#ifndef SFLPHONETRAY_H
#define SFLPHONETRAY_H

#include <KSystemTrayIcon>

//KDE
class KAction;

//Qt
class QMenu;
class QIcon;

///SFLPhoneTray: The old system try, should be totally replaced by a plasmoid some day
class SFLPhoneTray : public KSystemTrayIcon
{
Q_OBJECT

public:
   //Constructor
   explicit SFLPhoneTray(QIcon icon, QWidget *parent = 0);
   ~SFLPhoneTray();
   
   //Mutators
   void addAction(KAction *action);
   void addSeparator();

private:
   //Attributes
   QMenu* m_pTrayIconMenu;
};

#endif // SFLPHONETRAY_H
