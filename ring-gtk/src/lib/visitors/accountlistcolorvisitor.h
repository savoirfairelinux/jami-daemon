/****************************************************************************
 *   Copyright (C) 2012 by Savoir-Faire Linux                               *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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

#ifndef ACCOUNTLISTCOLORVISITOR_H
#define ACCOUNTLISTCOLORVISITOR_H

#include "../typedefs.h"
class Account;

///SFLPhonelib Qt does not link to QtGui, and does not need to, this allow to add runtime Gui support
class LIB_EXPORT AccountListColorVisitor {
public:
   virtual QVariant getColor(const Account* a) = 0;
   virtual QVariant getIcon(const Account* a)  = 0;
   virtual ~AccountListColorVisitor() {}
};

#endif
