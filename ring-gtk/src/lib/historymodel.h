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
#ifndef HISTORY_MODEL_H
#define HISTORY_MODEL_H
//Base
#include "typedefs.h"
#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QStringList>

//Qt

//SFLPhone
#include "call.h"
#include "commonbackendmanagerinterface.h"

//Typedef
typedef QMap<uint, Call*>  CallMap;
typedef QList<Call*>       CallList;

class HistoryItemNode;
class AbstractHistoryBackend;

///HistoryModel: History call manager
class LIB_EXPORT HistoryModel : public QAbstractItemModel, public CommonBackendManagerInterface<AbstractHistoryBackend> {
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
public:
   friend class HistoryItemNode;

   //Properties
   Q_PROPERTY(bool hasBackends   READ hasBackends  )

   //Singleton
   static HistoryModel* instance();

   //Getters
   int  acceptedPayloadTypes       () const;
   bool isHistoryLimited           () const;
   int  historyLimit               () const;
   virtual bool hasBackends        () const;
   virtual bool hasEnabledBackends () const;
   const CallMap getHistoryCalls() const;
   virtual const QVector<AbstractHistoryBackend*> backends() const;
   virtual const QVector<AbstractHistoryBackend*> enabledBackends() const;
   virtual CommonItemBackendModel* backendModel() const;

   //Setters
   void setCategoryRole(Call::Role role);
   void setHistoryLimited(bool isLimited);
   void setHistoryLimit(int numberOfDays);

   //Mutator
   void addBackend(AbstractHistoryBackend* backend, LoadOptions options = LoadOptions::NONE);
   void clearAllBackends() const;
   virtual bool enableBackend(AbstractHistoryBackend* backend, bool enabled);

   //Model implementation
   virtual bool          setData     ( const QModelIndex& index, const QVariant &value, int role   );
   virtual QVariant      data        ( const QModelIndex& index, int role = Qt::DisplayRole        ) const;
   virtual int           rowCount    ( const QModelIndex& parent = QModelIndex()                   ) const;
   virtual Qt::ItemFlags flags       ( const QModelIndex& index                                    ) const;
   virtual int           columnCount ( const QModelIndex& parent = QModelIndex()                   ) const __attribute__ ((const));
   virtual QModelIndex   parent      ( const QModelIndex& index                                    ) const;
   virtual QModelIndex   index       ( int row, int column, const QModelIndex& parent=QModelIndex()) const;
   virtual QVariant      headerData  ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
   virtual QStringList   mimeTypes   (                                                             ) const;
   virtual QMimeData*    mimeData    ( const QModelIndexList &indexes                              ) const;
   virtual bool          dropMimeData( const QMimeData*, Qt::DropAction, int, int, const QModelIndex& );
   virtual bool          insertRows  ( int row, int count, const QModelIndex & parent = QModelIndex() );


private:
   class TopLevelItem;

   //Model
   class HistoryItem : public CategorizedCompositeNode {
   public:
      explicit HistoryItem(Call* call);
      virtual ~HistoryItem();
      virtual QObject* getSelf() const;
      Call* call() const;
      int m_Index;
      TopLevelItem* m_pParent;
      HistoryItemNode* m_pNode;
   private:
      Call* m_pCall;
   };

   class TopLevelItem : public CategorizedCompositeNode,public QObject {
   friend class HistoryModel;
   public:
      virtual QObject* getSelf() const;
      virtual ~TopLevelItem();
      int m_Index;
      int m_AbsoluteIndex;
      QVector<HistoryItem*> m_lChildren;
   private:
      explicit TopLevelItem(const QString& name, int index);
      QString m_NameStr;
      int modelRow;
   };

   //Constructor
   explicit HistoryModel();
   ~HistoryModel();

   //Helpers
   TopLevelItem* getCategory(const Call* call);

   //Static attributes
   static HistoryModel* m_spInstance;

   //Attributes
   static CallMap m_sHistoryCalls;
   QVector<AbstractHistoryBackend*> m_lBackends;

   //Model categories
   QList<TopLevelItem*>         m_lCategoryCounter ;
   QHash<int,TopLevelItem*>     m_hCategories      ;
   QHash<QString,TopLevelItem*> m_hCategoryByName  ;
   int                          m_Role             ;
   QStringList                  m_lMimes           ;

public Q_SLOTS:
   void add(Call* call);

private Q_SLOTS:
   void reloadCategories();
   void slotChanged(const QModelIndex& idx);

Q_SIGNALS:
   ///Emitted when the history change (new items, cleared)
   void historyChanged          (            );
   ///Emitted when a new item is added to prevent full reload
   void newHistoryCall          ( Call* call );
   void newBackendAdded(AbstractHistoryBackend* backend);
};

class HistoryItemNode : public QObject //TODO remove this once Qt4 support is dropped
{
   Q_OBJECT
public:
   HistoryItemNode(HistoryModel* m, Call* c, HistoryModel::HistoryItem* backend);
   Call* m_pCall;
private:
   HistoryModel::HistoryItem* m_pBackend;
   HistoryModel* m_pModel;
private Q_SLOTS:
   void slotNumberChanged();
Q_SIGNALS:
   void changed(const QModelIndex& idx);
};

#endif
