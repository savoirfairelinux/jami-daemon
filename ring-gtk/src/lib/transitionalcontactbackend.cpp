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
#include "transitionalcontactbackend.h"

AbstractContactBackend* TransitionalContactBackend::m_spInstance = nullptr;

AbstractContactBackend* TransitionalContactBackend::instance()
{
   if (!m_spInstance) {
      m_spInstance = new TransitionalContactBackend();
   }
   return m_spInstance;
}

TransitionalContactBackend::~TransitionalContactBackend()
{
}

TransitionalContactBackend::TransitionalContactBackend(QObject* parent) : AbstractContactBackend(nullptr,parent)
{
}

bool TransitionalContactBackend::load()
{
   return false;
}

bool TransitionalContactBackend::reload()
{
   return false;
}

bool TransitionalContactBackend::append(const Contact* item)
{
   Q_UNUSED(item)
   return false;
}

bool TransitionalContactBackend::save(const Contact* contact)
{
   Q_UNUSED(contact)
   return false;
}

bool TransitionalContactBackend::edit( Contact* contact)
{
   Q_UNUSED(contact)
   return false;
}

bool TransitionalContactBackend::addNew( Contact* contact)
{
   Q_UNUSED(contact)
   return false;
}

bool TransitionalContactBackend::addPhoneNumber( Contact* contact , PhoneNumber* number )
{
   Q_UNUSED(contact)
   Q_UNUSED(number)
   return false;
}

bool TransitionalContactBackend::isEnabled() const
{
   return false;
}

AbstractContactBackend::SupportedFeatures TransitionalContactBackend::supportedFeatures() const
{
   return AbstractContactBackend::SupportedFeatures::NONE;
}

QString TransitionalContactBackend::name () const
{
   return "Transitional contact backend";
}

QVariant TransitionalContactBackend::icon() const
{
   return QVariant();
}

QByteArray TransitionalContactBackend::id() const
{
   return  "trcb";
}

QList<Contact*> TransitionalContactBackend::items() const
{
   return QList<Contact*>();
}
