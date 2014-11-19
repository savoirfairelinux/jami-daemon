/****************************************************************************
 *   Copyright (C) 2013-2014 by Savoir-Faire Linux                          *
 *   Author : Alexandre Lision <alexandre.lision@savoirfairelinux.com> *
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

#ifndef PROFILEPERSISTERVISITOR_H
#define PROFILEPERSISTERVISITOR_H

#include "../typedefs.h"

//Qt
#include <QtCore/QVariant>
#include <QtCore/QModelIndex>
#include <QDir>

//SFLPhone
class Contact;

class LIB_EXPORT ProfilePersisterVisitor {
public:
   virtual ~ProfilePersisterVisitor() {}
   virtual bool load();
   virtual bool save(const Contact* c);
   virtual QDir getProfilesDir();

   //Singleton
   static ProfilePersisterVisitor* instance();
   static void setInstance(ProfilePersisterVisitor* i);

protected:
   static ProfilePersisterVisitor* m_spInstance;
};

#endif // PROFILEPERSISTERVISITOR_H
