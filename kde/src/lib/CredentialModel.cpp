/************************************************************************************
 *   Copyright (C) 2012 by Savoir-Faire Linux                                       *
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
#include "CredentialModel.h"

#include <QtCore/QDebug>

///Constructor
CredentialModel::CredentialModel(QObject* parent) : QAbstractListModel(parent) {

}

///Model data
QVariant CredentialModel::data(const QModelIndex& index, int role) const {
   if(index.column() == 0 && role == Qt::DisplayRole)
      return QVariant(m_lCredentials[index.row()]->name);
//       else if(index.column() == 0 && role == Qt::CheckStateRole)
//          return QVariant(account->isEnabled() ? Qt::Checked : Qt::Unchecked);
   else if (index.column() == 0 && role == CredentialModel::NAME_ROLE) {
      return m_lCredentials[index.row()]->name;
   }
   else if (index.column() == 0 && role == CredentialModel::PASSWORD_ROLE) {
      return m_lCredentials[index.row()]->password;
   }
   else if (index.column() == 0 && role == CredentialModel::REALM_ROLE) {
      return m_lCredentials[index.row()]->realm;
   }
   return QVariant();
}

///Number of credentials
int CredentialModel::rowCount(const QModelIndex& parent) const {
   Q_UNUSED(parent)
   return m_lCredentials.size();
}

///Model flags
Qt::ItemFlags CredentialModel::flags(const QModelIndex& index) const {
   if (index.column() == 0)
      return QAbstractItemModel::flags(index) /*| Qt::ItemIsUserCheckable*/ | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
   return QAbstractItemModel::flags(index);
}

///Set credential data
bool CredentialModel::setData( const QModelIndex& index, const QVariant &value, int role) {
   if (index.column() == 0 && role == CredentialModel::NAME_ROLE) {
      m_lCredentials[index.row()]->name = value.toString();
      emit dataChanged(index, index);
      return true;
   }
   else if (index.column() == 0 && role == CredentialModel::PASSWORD_ROLE) {
      m_lCredentials[index.row()]->password = value.toString();
      emit dataChanged(index, index);
      return true;
   }
   else if (index.column() == 0 && role == CredentialModel::REALM_ROLE) {
      m_lCredentials[index.row()]->realm = value.toString();
      emit dataChanged(index, index);
      return true;
   }
   return false;
}

///Add a new credential
QModelIndex CredentialModel::addCredentials() {
   m_lCredentials << new CredentialData2;
   emit dataChanged(index(m_lCredentials.size()-1,0), index(m_lCredentials.size()-1,0));
   return index(m_lCredentials.size()-1,0);
}

///Remove credential at 'idx'
void CredentialModel::removeCredentials(QModelIndex idx) {
   qDebug() << "REMOVING" << idx.row() << m_lCredentials.size();
   if (idx.isValid()) {
      m_lCredentials.removeAt(idx.row());
      emit dataChanged(idx, index(m_lCredentials.size()-1,0));
      qDebug() << "DONE" << m_lCredentials.size();
   }
   else {
      qDebug() << "Failed to remove an invalid credential";
   }
}