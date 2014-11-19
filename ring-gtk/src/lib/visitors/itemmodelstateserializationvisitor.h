/****************************************************************************
 *   Copyright (C) 2014 by Savoir-Faire Linux                               *
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

#ifndef ITEMMODELSTATESERIALIZATIONVISITOR_H
#define ITEMMODELSTATESERIALIZATIONVISITOR_H

#include "../typedefs.h"
class AbstractItemBackendBase;
class Account;

///SFLPhonelib Qt does not link to QtGui, and does not need to, this allow to add runtime Gui support
class LIB_EXPORT ItemModelStateSerializationVisitor {
public:
   virtual bool save() = 0;
   virtual bool load() = 0;
   virtual ~ItemModelStateSerializationVisitor() {}

   static void setInstance(ItemModelStateSerializationVisitor* i);
   static ItemModelStateSerializationVisitor* instance();

   //Getter
   virtual bool isChecked(AbstractItemBackendBase* backend) const = 0;

   //Setter
   virtual bool setChecked(AbstractItemBackendBase* backend, bool enabled) = 0;

private:
   static ItemModelStateSerializationVisitor* m_spInstance;
};

#endif
