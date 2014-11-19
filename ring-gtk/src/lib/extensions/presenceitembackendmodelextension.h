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
#ifndef PRESENCEITEMBACKENDMODELEXTENSION_H
#define PRESENCEITEMBACKENDMODELEXTENSION_H

#include "../abstractitembackendmodelextension.h"

#include "../typedefs.h"

#include <QtCore/QVariant>
#include <QtCore/QModelIndex>

class AbstractContactBackend;

class LIB_EXPORT PresenceItemBackendModelExtension : public AbstractItemBackendModelExtension
{
   Q_OBJECT

public:
   PresenceItemBackendModelExtension(QObject* parent);

   virtual QVariant      data    (AbstractContactBackend* backend, const QModelIndex& index, int role = Qt::DisplayRole      ) const override;
   virtual Qt::ItemFlags flags   (AbstractContactBackend* backend, const QModelIndex& index                                  ) const override;
   virtual bool          setData (AbstractContactBackend* backend, const QModelIndex& index, const QVariant &value, int role ) override;
   virtual QString       headerName() const override;
};

#endif