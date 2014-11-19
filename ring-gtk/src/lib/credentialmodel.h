/****************************************************************************
 *   Copyright (C) 2012-2014 by Savoir-Faire Linux                          *
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
#ifndef CREDENTIAL_MODEL_H
#define CREDENTIAL_MODEL_H

#include <QtCore/QString>
#include <QtCore/QAbstractListModel>
#include "typedefs.h"


///CredentialModel: A model for account credentials
class LIB_EXPORT CredentialModel : public QAbstractListModel {
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
public:
   //Roles
   enum Role {
      NAME     = 100,
      PASSWORD = 101,
      REALM    = 102,
   };

   //Constructor
   explicit CredentialModel(QObject* parent = nullptr);
   virtual ~CredentialModel();

   //Abstract model member
   QVariant data(const QModelIndex& index, int role = Qt::DisplayRole ) const;
   int rowCount(const QModelIndex& parent = QModelIndex()             ) const;
   Qt::ItemFlags flags(const QModelIndex& index                       ) const;
   virtual bool setData(const QModelIndex& index, const QVariant &value, int role);

   //Mutator
   QModelIndex addCredentials();
   void removeCredentials(QModelIndex idx);
   void clear();

private:
   ///@struct CredentialData store credential information
   struct CredentialData2 {
      QString          name    ;
      QString          password;
      QString          realm   ;
   };

   //Attributes
   QList<CredentialData2*> m_lCredentials;
};
Q_DECLARE_METATYPE(CredentialModel*)

#endif
