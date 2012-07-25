/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
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
#ifndef SFLPHONE_ACCESSIBILITY_H
#define SFLPHONE_ACCESSIBILITY_H

#include <QtCore/QList>
#include <KAction>

///SFLPhoneAccessibility: Functions to improve accessibility for disabled peoples
class SFLPhoneAccessibility : public QObject, public QList<KAction*>
{
   Q_OBJECT
public:
   //Singleton
   static SFLPhoneAccessibility* getInstance();
private:
   //Constructor
   SFLPhoneAccessibility();
   QString numberToDigit(QString number);

   //Static attributes
   static SFLPhoneAccessibility* m_pInstance;

public slots:
   void listCall();
   void currentCallDetails();
   void say(QString message);
};

#endif
