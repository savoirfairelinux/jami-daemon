/****************************************************************************
 *   Copyright (C) 2013-2014 by Savoir-Faire Linux                          *
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

#ifndef PHONENUMBERSELECTOR_H
#define PHONENUMBERSELECTOR_H

#include "../typedefs.h"
#include "../contact.h"

class PhoneNumber;
class Contact;

///Common point visitor for UI specific contact dialog
class LIB_EXPORT PhoneNumberSelector {
public:
   virtual ~PhoneNumberSelector() {}
   virtual PhoneNumber* getNumber(const Contact* nb) = 0;
   static PhoneNumberSelector* defaultVisitor();
protected:
   static void setDefaultVisitor(PhoneNumberSelector* v);
private:
   static PhoneNumberSelector* m_spDefaultVisitor;
};

#endif
