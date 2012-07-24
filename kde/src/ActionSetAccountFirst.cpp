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
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 **************************************************************************/

//Parent
#include "ActionSetAccountFirst.h"

//KDE
#include <KLocale>

///Constrctor
ActionSetAccountFirst::ActionSetAccountFirst(Account* account, QObject *parent)
 : QAction((account == nullptr) ? i18n("Default account") : account->getAlias(), parent)
{
   setCheckable(true);
   this->account = account;
   connect(this,    SIGNAL(triggered()), this,    SLOT(emitSetFirst()));
}

///Destructor
ActionSetAccountFirst::~ActionSetAccountFirst()
{
}

///
void ActionSetAccountFirst::emitSetFirst()
{
   emit setFirst(account);
}
