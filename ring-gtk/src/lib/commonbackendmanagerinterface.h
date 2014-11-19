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
#ifndef COMMONBACKENDMANAGERINTERFACE_H
#define COMMONBACKENDMANAGERINTERFACE_H

#include "typedefs.h"

class CommonItemBackendModel;

enum LoadOptions {
   NONE           = 0x0     ,
   FORCE_ENABLED  = 0x1 << 0,
   FORCE_DISABLED = 0x1 << 1,
};

template <class T> class LIB_EXPORT CommonBackendManagerInterface {
public:
   virtual ~CommonBackendManagerInterface() {};



   /// Add a new backend
   virtual void addBackend(T* backend, LoadOptions options = LoadOptions::NONE) = 0;

   /// Do this manager have active backends
   virtual bool hasEnabledBackends () const = 0;
   virtual bool hasBackends        () const = 0;

   /// List all backends
   virtual const QVector<T*> backends       () const = 0;
   virtual const QVector<T*> enabledBackends() const = 0;

   ///Enable / disable a backend
   virtual bool enableBackend(T* backend, bool enabled) = 0;

   /// Return a backend
   virtual CommonItemBackendModel* backendModel() const = 0;
};

#endif
