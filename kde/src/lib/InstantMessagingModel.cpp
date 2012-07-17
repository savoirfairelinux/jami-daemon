/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
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
#include "InstantMessagingModel.h"

InstantMessagingModel::InstantMessagingModel(QObject* parent) : QAbstractListModel(parent)
{
   
}

QVariant InstantMessagingModel::data( const QModelIndex& index, int role) const
{
   if(index.column() == 0 && role == Qt::DisplayRole)
      return QVariant(m_lMessages[index.row()].message);
   else if (index.column() == 0 && role == MESSAGE_TYPE_ROLE    )
      return QVariant(m_lMessages[index.row()].message);
   else if (index.column() == 0 && role == MESSAGE_FROM_ROLE    )
      return QVariant(m_lMessages[index.row()].from);
   else if (index.column() == 0 && role == MESSAGE_TEXT_ROLE    )
      return INCOMMING_IM;
   else if (index.column() == 0 && role == MESSAGE_CONTACT_ROLE )
      return QVariant();
   return QVariant();
}

int InstantMessagingModel::rowCount(const QModelIndex& parent) const
{
   Q_UNUSED(parent)
   return m_lMessages.size();
}

Qt::ItemFlags InstantMessagingModel::flags(const QModelIndex& index) const
{
   Q_UNUSED(index)
   return Qt::ItemIsEnabled;
}

bool InstantMessagingModel::setData(const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED(index)
   Q_UNUSED(value)
   Q_UNUSED(role)
   return false;
}