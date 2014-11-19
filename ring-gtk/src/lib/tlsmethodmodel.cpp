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
#include "tlsmethodmodel.h"

#include <QtCore/QCoreApplication>

TlsMethodModel* TlsMethodModel::m_spInstance = nullptr;

TlsMethodModel::TlsMethodModel() : QAbstractListModel(QCoreApplication::instance()) {}

//Model functions
QVariant TlsMethodModel::data( const QModelIndex& index, int role) const
{
   if (!index.isValid()) return QVariant();
   TlsMethodModel::Type method = static_cast<TlsMethodModel::Type>(index.row());
   if (role == Qt::DisplayRole) {
      switch (method) {
         case TlsMethodModel::Type::DEFAULT:
            return TlsMethodModel::Name::DEFAULT;
         case TlsMethodModel::Type::TLSv1:
            return TlsMethodModel::Name::TLSv1;
         case TlsMethodModel::Type::SSLv3:
            return TlsMethodModel::Name::SSLv3;
         case TlsMethodModel::Type::SSLv23:
            return TlsMethodModel::Name::SSLv23;
      };
   }
   return QVariant();
}

int TlsMethodModel::rowCount( const QModelIndex& parent ) const
{
   return parent.isValid()?0:4;
}

Qt::ItemFlags TlsMethodModel::flags( const QModelIndex& index ) const
{
   if (!index.isValid()) return Qt::NoItemFlags;
   return Qt::ItemIsEnabled|Qt::ItemIsSelectable;
}

bool TlsMethodModel::setData( const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED(index)
   Q_UNUSED(value)
   Q_UNUSED(role )
   return false;
}

TlsMethodModel* TlsMethodModel::instance()
{
   if (!m_spInstance)
      m_spInstance = new TlsMethodModel();
   return m_spInstance;
}

///Translate enum type to QModelIndex
QModelIndex TlsMethodModel::toIndex(TlsMethodModel::Type type)
{
   return index(static_cast<int>(type),0,QModelIndex());
}

const char* TlsMethodModel::toDaemonName(TlsMethodModel::Type type)
{
   switch (type) {
      case TlsMethodModel::Type::DEFAULT:
         return TlsMethodModel::DaemonName::DEFAULT;
      case TlsMethodModel::Type::TLSv1:
         return TlsMethodModel::DaemonName::TLSv1;
      case TlsMethodModel::Type::SSLv3:
         return TlsMethodModel::DaemonName::SSLv3;
      case TlsMethodModel::Type::SSLv23:
         return TlsMethodModel::DaemonName::SSLv23;
   };
   return TlsMethodModel::DaemonName::DEFAULT;
}

TlsMethodModel::Type TlsMethodModel::fromDaemonName(const QString& name)
{
   if (name.isEmpty() || name == TlsMethodModel::DaemonName::DEFAULT)
      return TlsMethodModel::Type::DEFAULT;
   else if (name == TlsMethodModel::DaemonName::TLSv1)
      return TlsMethodModel::Type::TLSv1;
   else if (name == TlsMethodModel::DaemonName::SSLv3)
      return TlsMethodModel::Type::SSLv3;
   else if (name == TlsMethodModel::DaemonName::SSLv23)
      return TlsMethodModel::Type::SSLv23;
   qDebug() << "Unknown TLS method" << name;
   return TlsMethodModel::Type::DEFAULT;
}
