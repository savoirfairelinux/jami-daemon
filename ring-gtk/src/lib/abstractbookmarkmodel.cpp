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
#include "abstractbookmarkmodel.h"

//Qt
#include <QtCore/QMimeData>

//SFLPhone
#include "historymodel.h"
#include "dbus/presencemanager.h"
#include "phonedirectorymodel.h"
#include "phonenumber.h"
#include "callmodel.h"
#include "call.h"
#include "uri.h"
#include "abstractitembackend.h"

static bool test = false;
//Model item/index
class NumberTreeBackend : public CategorizedCompositeNode
{
   friend class AbstractBookmarkModel;
   public:
      NumberTreeBackend(PhoneNumber* number): CategorizedCompositeNode(CategorizedCompositeNode::Type::BOOKMARK),
         m_pNumber(number),m_pParent(nullptr),m_pNode(nullptr),m_Index(-1){
         Q_ASSERT(number != nullptr);
      }
      virtual ~NumberTreeBackend() {
         delete m_pNode;
      }
      virtual QObject* getSelf() const { return nullptr; }

      PhoneNumber* m_pNumber;
      AbstractBookmarkModel::TopLevelItem* m_pParent;
      int m_Index;
      BookmarkItemNode* m_pNode;
};


BookmarkItemNode::BookmarkItemNode(AbstractBookmarkModel* m, PhoneNumber* n, NumberTreeBackend* backend) :
m_pNumber(n),m_pBackend(backend),m_pModel(m){
   connect(n,SIGNAL(changed()),this,SLOT(slotNumberChanged()));
}

void BookmarkItemNode::slotNumberChanged()
{
   emit changed(m_pModel->index(m_pBackend->m_Index,0,m_pModel->index(m_pBackend->m_pParent->m_Row,0)));
}

QObject* AbstractBookmarkModel::TopLevelItem::getSelf() const
{
   return nullptr;
}

AbstractBookmarkModel::AbstractBookmarkModel(QObject* parent) : QAbstractItemModel(parent){
   setObjectName("AbstractBookmarkModel");
   reloadCategories();
   m_lMimes << MIME_PLAIN_TEXT << MIME_PHONENUMBER;

   //Connect
   connect(&DBus::PresenceManager::instance(),SIGNAL(newServerSubscriptionRequest(QString)),this,SLOT(slotRequest(QString)));
//    if (Call::contactBackend()) {
//       connect(Call::contactBackend(),SIGNAL(collectionChanged()),this,SLOT(reloadCategories()));
//    } //TODO implement reordering
}


///Reload bookmark cateogries
void AbstractBookmarkModel::reloadCategories()
{
   test = true;
   beginResetModel();
   m_hCategories.clear();
   foreach(TopLevelItem* item, m_lCategoryCounter) {
      foreach (NumberTreeBackend* child, item->m_lChildren) {
         delete child;
      }
      delete item;
   }
   m_lCategoryCounter.clear();

   //Load most used contacts
   if (displayFrequentlyUsed()) {
      TopLevelItem* item = new TopLevelItem(tr("Most popular"));
      m_hCategories["mp"] = item;
      item->m_Row = m_lCategoryCounter.size();
      item->m_MostPopular = true;
      m_lCategoryCounter << item;
      const QVector<PhoneNumber*> cl = PhoneDirectoryModel::instance()->getNumbersByPopularity();
      for (int i=0;i<((cl.size()>=10)?10:cl.size());i++) {
         PhoneNumber* n = cl[i];
         NumberTreeBackend* bm = new NumberTreeBackend(n);
         bm->m_pParent = item;
         bm->m_Index = item->m_lChildren.size();
         bm->m_pNode = new BookmarkItemNode(this,n,bm);
         connect(bm->m_pNode,SIGNAL(changed(QModelIndex)),this,SLOT(slotIndexChanged(QModelIndex)));
         item->m_lChildren << bm;
      }
   }

   foreach(PhoneNumber* bookmark, bookmarkList()) {
      NumberTreeBackend* bm = new NumberTreeBackend(bookmark);
      const QString val = category(bm);
      if (!m_hCategories[val]) {
         TopLevelItem* item = new TopLevelItem(val);
         m_hCategories[val] = item;
         item->m_Row = m_lCategoryCounter.size();
         m_lCategoryCounter << item;
      }
      TopLevelItem* item = m_hCategories[val];
      if (item) {
         bookmark->setBookmarked(true);
         bm->m_pParent = item;
         bm->m_Index = item->m_lChildren.size();
         bm->m_pNode = new BookmarkItemNode(this,bookmark,bm);
         connect(bm->m_pNode,SIGNAL(changed(QModelIndex)),this,SLOT(slotIndexChanged(QModelIndex)));
         item->m_lChildren << bm;
      }
      else
         qDebug() << "ERROR count";
   }
   endResetModel();
   emit layoutAboutToBeChanged();
   test = false;
   emit layoutChanged();
} //reloadCategories

//Do nothing
bool AbstractBookmarkModel::setData( const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED(index)
   Q_UNUSED(value)
   Q_UNUSED(role)
   return false;
}

///Get bookmark model data CategorizedCompositeNode::Type and Call::Role
QVariant AbstractBookmarkModel::data( const QModelIndex& index, int role) const
{
   if (!index.isValid() || test)
      return QVariant();

   CategorizedCompositeNode* modelItem = static_cast<CategorizedCompositeNode*>(index.internalPointer());
   if (!modelItem)
      return QVariant();
   switch (modelItem->type()) {
      case CategorizedCompositeNode::Type::TOP_LEVEL:
         switch (role) {
            case Qt::DisplayRole:
               return static_cast<TopLevelItem*>(modelItem)->m_Name;
            case Call::Role::Name:
               if (static_cast<TopLevelItem*>(modelItem)->m_MostPopular) {
                  return "000000";
               }
               else {
                  return static_cast<TopLevelItem*>(modelItem)->m_Name;
               }
         }
         break;
      case CategorizedCompositeNode::Type::BOOKMARK:
         return commonCallInfo(static_cast<NumberTreeBackend*>(modelItem),role);
         break;
      case CategorizedCompositeNode::Type::CALL:
      case CategorizedCompositeNode::Type::NUMBER:
      case CategorizedCompositeNode::Type::CONTACT:
         break;
   };
   return QVariant();
} //Data

///Get header data
QVariant AbstractBookmarkModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   Q_UNUSED(section)
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
      return QVariant(tr("Contacts"));
   return QVariant();
}


///Get the number of child of "parent"
int AbstractBookmarkModel::rowCount( const QModelIndex& parent ) const
{
   if (test) return 0; //HACK
   if (!parent.isValid())
      return m_lCategoryCounter.size();
   else if (!parent.parent().isValid() && parent.row() < m_lCategoryCounter.size()) {
      TopLevelItem* item = static_cast<TopLevelItem*>(parent.internalPointer());
      return item->m_lChildren.size();
   }
   return 0;
}

Qt::ItemFlags AbstractBookmarkModel::flags( const QModelIndex& index ) const
{
   if (!index.isValid())
      return 0;
   return Qt::ItemIsEnabled | Qt::ItemIsSelectable | (index.parent().isValid()?Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled:Qt::ItemIsEnabled);
}

///There is only 1 column
int AbstractBookmarkModel::columnCount ( const QModelIndex& parent) const
{
   Q_UNUSED(parent)
   return 1;
}

///Get the bookmark parent
QModelIndex AbstractBookmarkModel::parent( const QModelIndex& idx) const
{
   if (!idx.isValid()) {
      return QModelIndex();
   }
   const CategorizedCompositeNode* modelItem = static_cast<CategorizedCompositeNode*>(idx.internalPointer());
   if (modelItem->type() == CategorizedCompositeNode::Type::BOOKMARK) {
      TopLevelItem* item = static_cast<const NumberTreeBackend*>(modelItem)->m_pParent;
      if (item) {
         return index(item->m_Row,0);
      }
   }
   return QModelIndex();
} //parent

///Get the index
QModelIndex AbstractBookmarkModel::index(int row, int column, const QModelIndex& parent) const
{
   if (parent.isValid())
      return createIndex(row,column,(void*) static_cast<CategorizedCompositeNode*>(m_lCategoryCounter[parent.row()]->m_lChildren[row]));
   else {
      return createIndex(row,column,(void*) static_cast<CategorizedCompositeNode*>(m_lCategoryCounter[row]));
   }
}

///Get bookmarks mime types
QStringList AbstractBookmarkModel::mimeTypes() const
{
   return m_lMimes;
}

///Generate mime data
QMimeData* AbstractBookmarkModel::mimeData(const QModelIndexList &indexes) const
{
   QMimeData *mimeData = new QMimeData();
   foreach (const QModelIndex &index, indexes) {
      if (index.isValid()) {
         QString text = data(index, Call::Role::Number).toString();
         mimeData->setData(MIME_PLAIN_TEXT , text.toUtf8());
         mimeData->setData(MIME_PHONENUMBER, text.toUtf8());
         return mimeData;
      }
   }
   return mimeData;
} //mimeData

///Return valid payload types
int AbstractBookmarkModel::acceptedPayloadTypes()
{
   return CallModel::DropPayloadType::CALL;
}

///Get call info TODO use Call:: one
QVariant AbstractBookmarkModel::commonCallInfo(NumberTreeBackend* number, int role) const
{
   if (!number)
      return QVariant();
   QVariant cat;
   switch (role) {
      case Qt::DisplayRole:
      case Call::Role::Name:
         cat = number->m_pNumber->contact()?number->m_pNumber->contact()->formattedName():number->m_pNumber->primaryName();
         break;
      case Qt::ToolTipRole:
         cat = number->m_pNumber->presenceMessage();
         break;
      case Call::Role::Number:
         cat = number->m_pNumber->uri();//call->getPeerPhoneNumber();
         break;
      case Call::Role::Direction2:
         cat = 4;//call->getHistoryState();
         break;
      case Call::Role::Date:
         cat = tr("N/A");//call->getStartTimeStamp();
         break;
      case Call::Role::Length:
         cat = tr("N/A");//call->getLength();
         break;
      case Call::Role::FormattedDate:
         cat = tr("N/A");//QDateTime::fromTime_t(call->getStartTimeStamp().toUInt()).toString();
         break;
      case Call::Role::HasRecording:
         cat = false;//call->hasRecording();
         break;
      case Call::Role::Historystate:
         cat = (int)Call::LegacyHistoryState::NONE;//call->getHistoryState();
         break;
      case Call::Role::FuzzyDate:
         cat = "N/A";//timeToHistoryCategory(QDateTime::fromTime_t(call->getStartTimeStamp().toUInt()).date());
         break;
      case Call::Role::PhoneNu:
         return QVariant::fromValue(const_cast<PhoneNumber*>(number->m_pNumber));
      case Call::Role::IsBookmark:
         return true;
      case Call::Role::Filter:
         return number->m_pNumber->uri()+number->m_pNumber->primaryName();
      case Call::Role::IsPresent:
         return number->m_pNumber->isPresent();
      case Call::Role::PhotoPtr:
         if (number->m_pNumber->contact())
            return number->m_pNumber->contact()->photo();
         cat = true;
         break;
   }
   return cat;
} //commonCallInfo

///Get category
QString AbstractBookmarkModel::category(NumberTreeBackend* number) const
{
   QString cat = commonCallInfo(number).toString();
   if (cat.size())
      cat = cat[0].toUpper();
   return cat;
}

void AbstractBookmarkModel::slotRequest(const QString& uri)
{
   Q_UNUSED(uri)
   qDebug() << "Presence Request" << uri << "denied";
   //DBus::PresenceManager::instance().answerServerRequest(uri,true); //FIXME turn on after 1.3.0
}



QVector<PhoneNumber*> AbstractBookmarkModel::serialisedToList(const QStringList& list)
{
   QVector<PhoneNumber*> numbers;
   foreach(const QString& item,list) {
      PhoneNumber* nb = PhoneDirectoryModel::instance()->fromHash(item);
      if (nb) {
         nb->setTracked(true);
         nb->setUid(item);
         numbers << nb;
      }
   }
   return numbers;
}

bool AbstractBookmarkModel::displayFrequentlyUsed() const
{
   return false;
}

QVector<PhoneNumber*> AbstractBookmarkModel::bookmarkList() const
{
   return QVector<PhoneNumber*>();
}

AbstractBookmarkModel::TopLevelItem::TopLevelItem(QString name)
   : CategorizedCompositeNode(CategorizedCompositeNode::Type::TOP_LEVEL),m_Name(name),
      m_MostPopular(false),m_Row(-1)
{
}

bool AbstractBookmarkModel::removeRows( int row, int count, const QModelIndex & parent)
{
   if (parent.isValid()) {
      const int parentRow = parent.row();
      beginRemoveRows(parent,row,row+count-1);
      for (int i=row;i<row+count;i++)
         m_lCategoryCounter[parent.row()]->m_lChildren.removeAt(i);
      endRemoveRows();
      if (!m_lCategoryCounter[parentRow]->m_lChildren.size()) {
         beginRemoveRows(QModelIndex(),parentRow,parentRow);
         m_hCategories.remove(m_hCategories.key(m_lCategoryCounter[parentRow]));
         m_lCategoryCounter.removeAt(parentRow);
         for (int i=0;i<m_lCategoryCounter.size();i++) {
            m_lCategoryCounter[i]->m_Row =i;
         }
         endRemoveRows();
      }
      return true;
   }
   return false;
}

void AbstractBookmarkModel::remove(const QModelIndex& idx)
{
   PhoneNumber* nb = getNumber(idx);
   if (nb) {
      removeRows(idx.row(),1,idx.parent());
      removeBookmark(nb);
      emit layoutAboutToBeChanged();
      emit layoutChanged();
   }
}

PhoneNumber* AbstractBookmarkModel::getNumber(const QModelIndex& idx)
{
   if (idx.isValid()) {
      if (idx.parent().isValid() && idx.parent().row() < m_lCategoryCounter.size()) {
         return m_lCategoryCounter[idx.parent().row()]->m_lChildren[idx.row()]->m_pNumber;
      }
   }
   return nullptr;
}

///Callback when an item change
void AbstractBookmarkModel::slotIndexChanged(const QModelIndex& idx)
{
   emit dataChanged(idx,idx);
}
