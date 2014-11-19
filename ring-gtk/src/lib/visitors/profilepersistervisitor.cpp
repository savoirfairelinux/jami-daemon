/****************************************************************************
 *   Copyright (C) 2013-2014 by Savoir-Faire Linux                           *
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

#include "profilepersistervisitor.h"
#include <QtCore/QSize>

ProfilePersisterVisitor* ProfilePersisterVisitor::m_spInstance = nullptr;

void ProfilePersisterVisitor::setInstance(ProfilePersisterVisitor* i)
{
   m_spInstance = i;
}

ProfilePersisterVisitor* ProfilePersisterVisitor::instance()
{
   return m_spInstance;
}

bool ProfilePersisterVisitor::load()
{
   return false;
}

bool ProfilePersisterVisitor::save(const Contact* c)
{
   Q_UNUSED(c)
   return false;
}

QDir ProfilePersisterVisitor::getProfilesDir()
{
   return *(new QDir());
}
