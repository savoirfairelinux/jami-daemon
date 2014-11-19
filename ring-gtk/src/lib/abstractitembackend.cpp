/************************************************************************************
 *   Copyright (C) 2014 by Savoir-Faire Linux                                       *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/

//Parent
#include "abstractitembackend.h"

//SFLPhone library
#include "contact.h"
#include "call.h"
#include "phonenumber.h"

//Qt
#include <QtCore/QHash>
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>

///Constructor
AbstractContactBackend::AbstractContactBackend(AbstractItemBackendInterface<Contact>* parentBackend,QObject* par)
: AbstractItemBackendInterface<Contact>(parentBackend),QObject(par?par:QCoreApplication::instance())
{
}

///Destructor
AbstractContactBackend::~AbstractContactBackend()
{
}

///Constructor
AbstractHistoryBackend::AbstractHistoryBackend(AbstractItemBackendInterface<Call>* parentBackend, QObject* par)
: AbstractItemBackendInterface<Call>(parentBackend),QObject(par?par:QCoreApplication::instance())
{
}

///Destructor
AbstractHistoryBackend::~AbstractHistoryBackend()
{
}

QVector<AbstractItemBackendBase*> AbstractItemBackendBase::baseChildrenBackends() const
{
   return m_lBaseChildren;
}


bool AbstractItemBackendBase::clear()
{
   return false;
}

///Default batch saving implementation, some backends have better APIs
template <class T> bool AbstractItemBackendInterface<T>::batchSave(const QList<T*> contacts)
{
   bool ret = true;
   foreach(const T* c, contacts) {
      ret &= save(c);
   }
   return ret;
}

template <class T> bool AbstractItemBackendInterface<T>::remove(T* item)
{
   Q_UNUSED(item)
   return false;
}

bool AbstractItemBackendBase::enable(bool)
{
   return false;
}
