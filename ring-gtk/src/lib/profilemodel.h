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
#ifndef PROFILEMODEL_H
#define PROFILEMODEL_H

#include "typedefs.h"
#include "contact.h"
#include "account.h"
#include <QStringList>
#include <QtCore/QAbstractItemModel>

class ProfileContentBackend;
class ProfilePersisterVisitor;
class VCardMapper;

typedef void (VCardMapper:: *mapToProperty)(Contact*, const QByteArray&);

class LIB_EXPORT ProfileModel : public QAbstractItemModel {
   Q_OBJECT
public:
   explicit ProfileModel(QObject* parent = nullptr);
   virtual ~ProfileModel();
   static ProfileModel* instance();

   //Abstract model member
   virtual QVariant data        (const QModelIndex& index, int role = Qt::DisplayRole         ) const;
   virtual int rowCount         (const QModelIndex& parent = QModelIndex()                    ) const;
   virtual int columnCount      (const QModelIndex& parent = QModelIndex()                    ) const;
   virtual Qt::ItemFlags flags  (const QModelIndex& index                                     ) const;
   virtual bool        setData         (const QModelIndex& index, const QVariant &value, int role    )      ;
   virtual QModelIndex index    (int row, int column, const QModelIndex& parent=QModelIndex() ) const;
   virtual QModelIndex parent   (const QModelIndex& index                                     ) const;
   virtual QVariant    headerData  (int section, Qt::Orientation orientation, int role           ) const;
   virtual QStringList mimeType () const;
   virtual QMimeData* mimeData(const QModelIndexList &indexes) const;
   virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);

   //Getter
   QModelIndex mapToSource  (const QModelIndex& idx) const;
   QModelIndex mapFromSource(const QModelIndex& idx) const;
   int acceptedPayloadTypes() const;

   //Attributes
   QStringList m_lMimes;

   AbstractContactBackend* getBackEnd();

private:

   //Singleton
   static ProfileModel*                   m_spInstance;
   ProfileContentBackend*                 m_pProfileBackend;
   ProfilePersisterVisitor*               m_pVisitor   ;

   //Helpers
   void updateIndexes();

public Q_SLOTS:
   bool addNewProfile(Contact* c, AbstractContactBackend* backend = nullptr);

private Q_SLOTS:
   void slotDataChanged(const QModelIndex& tl,const QModelIndex& br);
   void slotLayoutchanged();

};

#endif
