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
#include "certificate.h"

#include <QtCore/QFile>


Certificate::Certificate(Certificate::Type type, const QObject* parent) : QObject(const_cast<QObject*>(parent)),m_Type(type)
{

}

bool Certificate::exist() const
{
   return QFile::exists(m_Path.toLocalFile());
}

QUrl Certificate::path() const
{
   return m_Path;
}

void Certificate::setPath(const QUrl& path)
{
   m_Path = path;
   emit changed();
}

bool Certificate::isExpired() const
{
   return true; //TODO
}

bool Certificate::isSelfSigned() const
{
   return true; //TODO
}

bool Certificate::hasPrivateKey() const
{
   return false; //TODO
}

bool Certificate::hasProtectedPrivateKey() const
{
   return false; //TODO
}

bool Certificate::hasRightPermissions() const
{
   return false; //TODO
}

bool Certificate::hasRightFolderPermissions() const
{
   return false; //TODO
}

bool Certificate::isLocationSecure() const
{
   return false; //TODO
}

Certificate::Type Certificate::type() const
{
   return m_Type;
}
