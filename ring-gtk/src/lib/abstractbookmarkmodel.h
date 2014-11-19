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
#ifndef ABSTRACTBOOKMARKMODEL_H
#define ABSTRACTBOOKMARKMODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QHash>
#include <QtCore/QStringList>
#include <QtCore/QDateTime>

//SFLPhone
#include "typedefs.h"
#include "contact.h"
#include "call.h"
class ContactBackend;
class NumberTreeBackend;

class LIB_EXPORT AbstractBookmarkModel :  public QAbstractItemModel
{
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
public:
   friend class NumberTreeBackend;
   //Constructor
   virtual ~AbstractBookmarkModel() {}
   explicit AbstractBookmarkModel(QObject* parent);

   //Setters
   void setRole(int role);
   void setShowAll(bool showAll);

   //Model implementation
   virtual bool          setData     ( const QModelIndex& index, const QVariant &value, int role   );
   virtual QVariant      data        ( const QModelIndex& index, int role = Qt::DisplayRole        ) const;
   virtual int           rowCount    ( const QModelIndex& parent = QModelIndex()                   ) const;
   virtual Qt::ItemFlags flags       ( const QModelIndex& index                                    ) const;
   virtual int           columnCount ( const QModelIndex& parent = QModelIndex()                   ) const;
   virtual QModelIndex   parent      ( const QModelIndex& index                                    ) const;
   virtual QModelIndex   index       ( int row, int column, const QModelIndex& parent=QModelIndex()) const;
   virtual QVariant      headerData  ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
   virtual QStringList   mimeTypes   (                                                             ) const;
   virtual QMimeData*    mimeData    ( const QModelIndexList &indexes                              ) const;
   virtual bool          removeRows  ( int row, int count, const QModelIndex & parent = QModelIndex() );

   //Management
   void         remove(const QModelIndex& idx                         );
   virtual void addBookmark   (PhoneNumber* number, bool trackPresence = false) = 0;
   virtual void removeBookmark(PhoneNumber* number                            ) = 0;

   //Getters
   int          acceptedPayloadTypes();
   PhoneNumber* getNumber(const QModelIndex& idx);

protected:
   virtual bool                  displayFrequentlyUsed() const;
   virtual QVector<PhoneNumber*> bookmarkList         () const;

   //Helpers
   static QVector<PhoneNumber*> serialisedToList(const QStringList& list);

private:
   ///Top level bookmark item
   class TopLevelItem : public CategorizedCompositeNode {
      friend class AbstractBookmarkModel;
      public:
         virtual QObject* getSelf() const;
         int m_Row;
      private:
         explicit TopLevelItem(QString name);
         QList<NumberTreeBackend*> m_lChildren;
         QString m_Name;
         bool m_MostPopular;
   };

   //Attributes
   QList<TopLevelItem*>         m_lCategoryCounter ;
   QHash<QString,TopLevelItem*> m_hCategories      ;
   QStringList                  m_lMimes           ;

   //Getters
   QModelIndex getContactIndex(Contact* ct) const;

   //Helpers
   QVariant commonCallInfo(NumberTreeBackend* call, int role = Qt::DisplayRole) const;
   QString category(NumberTreeBackend* number) const;

private Q_SLOTS:
   void slotRequest(const QString& uri);
   void slotIndexChanged(const QModelIndex& idx);

public Q_SLOTS:
   void reloadCategories();
};

class BookmarkItemNode : public QObject //TODO remove this once Qt4 support is dropped
{
   Q_OBJECT
public:
   BookmarkItemNode(AbstractBookmarkModel* m, PhoneNumber* n, NumberTreeBackend* backend);
private:
   PhoneNumber* m_pNumber;
   NumberTreeBackend* m_pBackend;
   AbstractBookmarkModel* m_pModel;
private Q_SLOTS:
   void slotNumberChanged();
Q_SIGNALS:
   void changed(const QModelIndex& idx);
};

#endif
