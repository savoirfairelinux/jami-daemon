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

#ifndef ACTION_SET_ACCOUNT_FIRST_H
#define ACTION_SET_ACCOUNT_FIRST_H

#include <QAction>

#include "lib/Account.h"

///ActionSetAccountFirst Set an account to be the first
class ActionSetAccountFirst : public QAction
{
Q_OBJECT

private:
   Account* account;

public:
   explicit ActionSetAccountFirst(Account * account, QObject *parent = 0);
   ~ActionSetAccountFirst();

private slots:
   void emitSetFirst();

signals:
   ///Set the account to be the first one
   void setFirst(Account * account);

};

#endif
