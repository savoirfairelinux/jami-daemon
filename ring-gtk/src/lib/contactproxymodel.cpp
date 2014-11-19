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
#include "contactproxymodel.h"

//Qt
#include <QtCore/QDebug>
#include <QtCore/QDate>
#include <QtCore/QMimeData>
#include <QtCore/QCoreApplication>

//SFLPhone
#include "callmodel.h"
#include "historymodel.h"
#include "phonenumber.h"
#include "phonedirectorymodel.h"
#include "historytimecategorymodel.h"
#include "contact.h"
#include "uri.h"
#include "contactmodel.h"

class ContactTreeNode;

class TopLevelItem : public CategorizedCompositeNode {
   friend class ContactProxyModel;
   friend class ContactTreeBinder;
   public:
      virtual QObject* getSelf() const;
      virtual ~TopLevelItem();
   private:
      explicit TopLevelItem(const QString& name) : CategorizedCompositeNode(CategorizedCompositeNode::Type::TOP_LEVEL),m_Name(name),
      m_lChildren(),m_Index(-1){
         m_lChildren.reserve(32);
      }
      QVector<ContactTreeNode*> m_lChildren;
      QString m_Name;
      int m_Index;
};

class ContactTreeNode : public CategorizedCompositeNode {
public:
   ContactTreeNode(Contact* ct, ContactProxyModel* parent);
   ~ContactTreeNode();
   Contact* m_pContact;
   TopLevelItem* m_pParent3;
   uint m_Index;
   virtual QObject* getSelf() const;
   ContactTreeBinder* m_pBinder;
};

TopLevelItem::~TopLevelItem() {
   while(m_lChildren.size()) {
      ContactTreeNode* node = m_lChildren[0];
      m_lChildren.remove(0);
      delete node;
   }
}

ContactTreeNode::ContactTreeNode(Contact* ct, ContactProxyModel* parent) : CategorizedCompositeNode(CategorizedCompositeNode::Type::CONTACT),
   m_pContact(ct),m_pParent3(nullptr),m_Index(-1)
{
   m_pBinder = new ContactTreeBinder(parent,this);
}

ContactTreeNode::~ContactTreeNode()
{
   delete m_pBinder;
}

QObject* ContactTreeNode::getSelf() const
{
   return m_pContact;
}

QObject* TopLevelItem::getSelf() const
{
   return nullptr;
}

ContactTreeBinder::ContactTreeBinder(ContactProxyModel* m,ContactTreeNode* n) :
   QObject(),m_pTreeNode(n),m_pModel(m)
{
   connect(n->m_pContact,SIGNAL(changed()),this,SLOT(slotContactChanged()));
   connect(n->m_pContact,SIGNAL(phoneNumberCountChanged(int,int)),this,SLOT(slotPhoneNumberCountChanged(int,int)));
   connect(n->m_pContact,SIGNAL(phoneNumberCountAboutToChange(int,int)),this,SLOT(slotPhoneNumberCountAboutToChange(int,int)));
}


void ContactTreeBinder::slotContactChanged()
{
   const QModelIndex idx = m_pModel->index(m_pTreeNode->m_Index,0,m_pModel->index(m_pTreeNode->m_pParent3->m_Index,0));
   const QModelIndex lastPhoneIdx = m_pModel->index(m_pTreeNode->m_pContact->phoneNumbers().size()-1,0,idx);
   emit m_pModel->dataChanged(idx,idx);
   if (lastPhoneIdx.isValid()) //Need to be done twice
      emit m_pModel->dataChanged(m_pModel->index(0,0,idx),lastPhoneIdx);
}

void ContactTreeBinder::slotStatusChanged()
{

}

void ContactTreeBinder::slotPhoneNumberCountChanged(int count, int oldCount)
{
   const QModelIndex idx = m_pModel->index(m_pTreeNode->m_Index,0,m_pModel->index(m_pTreeNode->m_pParent3->m_Index,0));
   if (count > oldCount) {
      const QModelIndex lastPhoneIdx = m_pModel->index(oldCount-1,0,idx);
      m_pModel->beginInsertRows(idx,oldCount,count-1);
      m_pModel->endInsertRows();
   }
   emit m_pModel->dataChanged(idx,idx);
}

void ContactTreeBinder::slotPhoneNumberCountAboutToChange(int count, int oldCount)
{
   const QModelIndex idx = m_pModel->index(m_pTreeNode->m_Index,0,m_pModel->index(m_pTreeNode->m_pParent3->m_Index,0));
   if (count < oldCount) {
      //If count == 1, disable all children
      m_pModel->beginRemoveRows(idx,count == 1?0:count,oldCount-1);
      m_pModel->endRemoveRows();
   }
}

//
ContactProxyModel::ContactProxyModel(int role, bool showAll) : QAbstractItemModel(QCoreApplication::instance()),
m_Role(role),m_ShowAll(showAll),m_lCategoryCounter()
{
   setObjectName("ContactProxyModel");
   m_lCategoryCounter.reserve(32);
   m_lMimes << MIME_PLAIN_TEXT << MIME_PHONENUMBER;
   connect(ContactModel::instance(),SIGNAL(reloaded()),this,SLOT(reloadCategories()));
   connect(ContactModel::instance(),SIGNAL(newContactAdded(Contact*)),this,SLOT(slotContactAdded(Contact*)));
   QHash<int, QByteArray> roles = roleNames();
   roles.insert(ContactModel::Role::Organization      ,QByteArray("organization")     );
   roles.insert(ContactModel::Role::Group             ,QByteArray("group")            );
   roles.insert(ContactModel::Role::Department        ,QByteArray("department")       );
   roles.insert(ContactModel::Role::PreferredEmail    ,QByteArray("preferredEmail")   );
   roles.insert(ContactModel::Role::FormattedLastUsed ,QByteArray("formattedLastUsed"));
   roles.insert(ContactModel::Role::IndexedLastUsed   ,QByteArray("indexedLastUsed")  );
   roles.insert(ContactModel::Role::DatedLastUsed     ,QByteArray("datedLastUsed")    );
   roles.insert(ContactModel::Role::Filter            ,QByteArray("filter")           );
   roles.insert(ContactModel::Role::DropState         ,QByteArray("dropState")        );
   setRoleNames(roles);
}

ContactProxyModel::~ContactProxyModel()
{
   foreach(TopLevelItem* item,m_lCategoryCounter) {
      delete item;
   }
}

TopLevelItem* ContactProxyModel::getTopLevelItem(const QString& category)
{
   if (!m_hCategories[category]) {
      TopLevelItem* item = new TopLevelItem(category);
      m_hCategories[category] = item;
      item->m_Index = m_lCategoryCounter.size();
//       emit layoutAboutToBeChanged();
      beginInsertRows(QModelIndex(),m_lCategoryCounter.size(),m_lCategoryCounter.size()); {
         m_lCategoryCounter << item;
      } endInsertRows();
//       emit layoutChanged();
   }
   TopLevelItem* item = m_hCategories[category];
   return item;
}

void ContactProxyModel::reloadCategories()
{
   emit layoutAboutToBeChanged();
   beginResetModel();
   m_hCategories.clear();
   beginRemoveRows(QModelIndex(),0,m_lCategoryCounter.size()-1);
   foreach(TopLevelItem* item,m_lCategoryCounter) {
      delete item;
   }
   endRemoveRows();
   m_lCategoryCounter.clear();
   foreach(const Contact* cont, ContactModel::instance()->contacts()) {
      if (cont) {
         const QString val = category(cont);
         TopLevelItem* item = getTopLevelItem(val);
         ContactTreeNode* contactNode = new ContactTreeNode(const_cast<Contact*>(cont),this);
         contactNode->m_pParent3 = item;
         contactNode->m_Index = item->m_lChildren.size();
         item->m_lChildren << contactNode;
      }
   }
   endResetModel();
   emit layoutChanged();
}

void ContactProxyModel::slotContactAdded(Contact* c)
{
   if (!c) return;
   const QString val = category(c);
   TopLevelItem* item = getTopLevelItem(val);
   ContactTreeNode* contactNode = new ContactTreeNode(c,this);
   contactNode->m_pParent3 = item;
   contactNode->m_Index = item->m_lChildren.size();
   //emit layoutAboutToBeChanged();
   beginInsertRows(index(item->m_Index,0,QModelIndex()),item->m_lChildren.size(),item->m_lChildren.size()); {
      item->m_lChildren << contactNode;
   } endInsertRows();
   //emit layoutChanged();
}

bool ContactProxyModel::setData( const QModelIndex& index, const QVariant &value, int role)
{
   if (index.isValid() && index.parent().isValid()) {
      CategorizedCompositeNode* modelItem = (CategorizedCompositeNode*)index.internalPointer();
      if (role == ContactModel::Role::DropState) {
         modelItem->setDropState(value.toInt());
         emit dataChanged(index, index);
         return true;
      }
   }
   return false;
}

QVariant ContactProxyModel::data( const QModelIndex& index, int role) const
{
   if (!index.isValid())
      return QVariant();

   CategorizedCompositeNode* modelItem = (CategorizedCompositeNode*)index.internalPointer();
   switch (modelItem->type()) {
      case CategorizedCompositeNode::Type::TOP_LEVEL:
      switch (role) {
         case Qt::DisplayRole:
            return static_cast<const TopLevelItem*>(modelItem)->m_Name;
         case ContactModel::Role::IndexedLastUsed:
            return index.child(0,0).data(ContactModel::Role::IndexedLastUsed);
         case ContactModel::Role::Active:
            return true;
         default:
            break;
      }
      break;
   case CategorizedCompositeNode::Type::CONTACT:{
      const Contact* c = static_cast<Contact*>(modelItem->getSelf());
      switch (role) {
         case Qt::DisplayRole:
            return QVariant(c->formattedName());
         case ContactModel::Role::Organization:
            return QVariant(c->organization());
         case ContactModel::Role::Group:
            return QVariant(c->group());
         case ContactModel::Role::Department:
            return QVariant(c->department());
         case ContactModel::Role::PreferredEmail:
            return QVariant(c->preferredEmail());
         case ContactModel::Role::DropState:
            return QVariant(modelItem->dropState());
         case ContactModel::Role::FormattedLastUsed:
            return QVariant(HistoryTimeCategoryModel::timeToHistoryCategory(c->phoneNumbers().lastUsedTimeStamp()));
         case ContactModel::Role::IndexedLastUsed:
            return QVariant((int)HistoryTimeCategoryModel::timeToHistoryConst(c->phoneNumbers().lastUsedTimeStamp()));
         case ContactModel::Role::Active:
            return c->isActive();
         case ContactModel::Role::DatedLastUsed:
            return QVariant(QDateTime::fromTime_t( c->phoneNumbers().lastUsedTimeStamp()));
         case ContactModel::Role::Filter:
            return c->filterString();
         default:
            break;
      }
      break;
   }
   case CategorizedCompositeNode::Type::NUMBER: /* && (role == Qt::DisplayRole)) {*/
   case CategorizedCompositeNode::Type::CALL:
   case CategorizedCompositeNode::Type::BOOKMARK:
   default:
      switch (role) {
         case ContactModel::Role::Active:
            return true;
      }
      break;
   };
   return QVariant();
}

QVariant ContactProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   Q_UNUSED(section)
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
      return QVariant(tr("Contacts"));
   return QVariant();
}

bool ContactProxyModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
   Q_UNUSED( action )
   setData(parent,-1,Call::Role::DropState);
   if (data->hasFormat(MIME_CALLID)) {
      const QByteArray encodedCallId = data->data( MIME_CALLID    );
      const QModelIndex targetIdx    = index   ( row,column,parent );
      Call* call                     = CallModel::instance()->getCall ( encodedCallId        );
      if (call && targetIdx.isValid()) {
         CategorizedCompositeNode* modelItem = (CategorizedCompositeNode*)targetIdx.internalPointer();
         switch (modelItem->type()) {
            case CategorizedCompositeNode::Type::CONTACT: {
               const Contact* ct = static_cast<Contact*>(modelItem->getSelf());
               if (ct) {
                  switch(ct->phoneNumbers().size()) {
                     case 0: //Do nothing when there is no phone numbers
                        return false;
                     case 1: //Call when there is one
                        CallModel::instance()->transfer(call,ct->phoneNumbers()[0]);
                        break;
                     default:
                        //TODO
                        break;
                  };
               }
            } break;
            case CategorizedCompositeNode::Type::NUMBER: {
               const Contact::PhoneNumbers nbs = *static_cast<Contact::PhoneNumbers*>(modelItem);
               const PhoneNumber*          nb  = nbs[row];
               if (nb) {
                  call->setTransferNumber(nb->uri());
                  CallModel::instance()->transfer(call,nb);
               }
            } break;
            case CategorizedCompositeNode::Type::CALL:
            case CategorizedCompositeNode::Type::BOOKMARK:
            case CategorizedCompositeNode::Type::TOP_LEVEL:
               break;
         }
      }
   }
   return false;
}


int ContactProxyModel::rowCount( const QModelIndex& parent ) const
{
   if (!parent.isValid() || !parent.internalPointer())
      return m_lCategoryCounter.size();
   const CategorizedCompositeNode* parentNode = static_cast<CategorizedCompositeNode*>(parent.internalPointer());
   switch(parentNode->type()) {
      case CategorizedCompositeNode::Type::TOP_LEVEL:
         return static_cast<const TopLevelItem*>(parentNode)->m_lChildren.size();
      case CategorizedCompositeNode::Type::CONTACT: {
         const Contact* ct = static_cast<Contact*>(parentNode->getSelf());
         const int size = ct->phoneNumbers().size();
         //Do not return the number if there is only one, it will be drawn part of the contact
         return size==1?0:size;
      }
      case CategorizedCompositeNode::Type::CALL:
      case CategorizedCompositeNode::Type::NUMBER:
      case CategorizedCompositeNode::Type::BOOKMARK:
      default:
         return 0;
   };
}

Qt::ItemFlags ContactProxyModel::flags( const QModelIndex& index ) const
{
   if (!index.isValid())
      return Qt::NoItemFlags;
   return Qt::ItemIsEnabled | Qt::ItemIsSelectable | (index.parent().isValid()?Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled:Qt::ItemIsEnabled);
}

int ContactProxyModel::columnCount ( const QModelIndex& parent) const
{
   Q_UNUSED(parent)
   return 1;
}

QModelIndex ContactProxyModel::parent( const QModelIndex& index) const
{
   if (!index.isValid() || !index.internalPointer())
      return QModelIndex();
   const CategorizedCompositeNode* modelItem = static_cast<CategorizedCompositeNode*>(index.internalPointer());
   switch (modelItem->type()) {
      case CategorizedCompositeNode::Type::CONTACT: {
         const TopLevelItem* tl = ((ContactTreeNode*)(modelItem))->m_pParent3;
         return createIndex(tl->m_Index,0,(void*)tl);
      }
      break;
      case CategorizedCompositeNode::Type::NUMBER: {
         const ContactTreeNode* parentNode = static_cast<const ContactTreeNode*>(modelItem->parentNode());
         return createIndex(parentNode->m_Index, 0, (void*)parentNode);
      }
      case CategorizedCompositeNode::Type::TOP_LEVEL:
      case CategorizedCompositeNode::Type::BOOKMARK:
      case CategorizedCompositeNode::Type::CALL:
      default:
         return QModelIndex();
         break;
   };
}

QModelIndex ContactProxyModel::index( int row, int column, const QModelIndex& parent) const
{
   if (parent.isValid() && parent.internalPointer()) {
      CategorizedCompositeNode* parentNode = static_cast<CategorizedCompositeNode*>(parent.internalPointer());
      switch(parentNode->type()) {
         case CategorizedCompositeNode::Type::TOP_LEVEL: {
            TopLevelItem* tld = static_cast<TopLevelItem*>(parentNode);
            if (tld && row < tld->m_lChildren.size())
               return createIndex(row,column,(void*)tld->m_lChildren[row]);
         }
            break;
         case CategorizedCompositeNode::Type::CONTACT: {
            const ContactTreeNode* ctn = (ContactTreeNode*)parentNode;
            const Contact*          ct = (Contact*)ctn->getSelf()    ;
            if (ct->phoneNumbers().size()>row) {
               const_cast<Contact::PhoneNumbers*>(&ct->phoneNumbers())->setParentNode((CategorizedCompositeNode*)ctn);
               return createIndex(row,column,(void*)&ct->phoneNumbers());
            }
         }
            break;
         case CategorizedCompositeNode::Type::CALL:
         case CategorizedCompositeNode::Type::BOOKMARK:
         case CategorizedCompositeNode::Type::NUMBER:
            break;
      };
   }
   else if (row < m_lCategoryCounter.size()){
      //Return top level
      return createIndex(row,column,(void*)m_lCategoryCounter[row]);
   }
   return QModelIndex();
}

QStringList ContactProxyModel::mimeTypes() const
{
   return m_lMimes;
}

QMimeData* ContactProxyModel::mimeData(const QModelIndexList &indexes) const
{
   QMimeData *mimeData = new QMimeData();
   foreach (const QModelIndex &index, indexes) {
      if (index.isValid()) {
         const CategorizedCompositeNode* modelItem = static_cast<CategorizedCompositeNode*>(index.internalPointer());
         switch(modelItem->type()) {
            case CategorizedCompositeNode::Type::CONTACT: {
               //Contact
               const Contact* ct = static_cast<Contact*>(modelItem->getSelf());
               if (ct) {
                  if (ct->phoneNumbers().size() == 1) {
                     mimeData->setData(MIME_PHONENUMBER , ct->phoneNumbers()[0]->toHash().toUtf8());
                  }
                  mimeData->setData(MIME_CONTACT , ct->uid());
               }
               return mimeData;
               } break;
            case CategorizedCompositeNode::Type::NUMBER: {
               //Phone number
               const QString text = data(index, Qt::DisplayRole).toString();
               const Contact::PhoneNumbers nbs = *static_cast<Contact::PhoneNumbers*>(index.internalPointer());
               const PhoneNumber*          nb  = nbs[index.row()];
               mimeData->setData(MIME_PLAIN_TEXT , text.toUtf8());
               mimeData->setData(MIME_PHONENUMBER, nb->toHash().toUtf8());
               return mimeData;
               } break;
            case CategorizedCompositeNode::Type::TOP_LEVEL:
            case CategorizedCompositeNode::Type::CALL:
            case CategorizedCompositeNode::Type::BOOKMARK:
            default:
               return nullptr;
         };
      }
   }
   return mimeData;
}

///Return valid payload types
int ContactProxyModel::acceptedPayloadTypes()
{
   return CallModel::DropPayloadType::CALL;
}



/*****************************************************************************
 *                                                                           *
 *                                  Helpers                                  *
 *                                                                           *
 ****************************************************************************/


QString ContactProxyModel::category(const Contact* ct) const {
   if (!ct)
      return QString();
   QString cat;
   switch (m_Role) {
      case ContactModel::Role::Organization:
         cat = ct->organization();
         break;
      case ContactModel::Role::Group:
         cat = ct->group();
         break;
      case ContactModel::Role::Department:
         cat = ct->department();
         break;
      case ContactModel::Role::PreferredEmail:
         cat = ct->preferredEmail();
         break;
      case ContactModel::Role::FormattedLastUsed:
         cat = HistoryTimeCategoryModel::timeToHistoryCategory(ct->phoneNumbers().lastUsedTimeStamp());
         break;
      case ContactModel::Role::IndexedLastUsed:
         cat = QString::number((int)HistoryTimeCategoryModel::timeToHistoryConst(ct->phoneNumbers().lastUsedTimeStamp()));
         break;
      case ContactModel::Role::DatedLastUsed:
         cat = QDateTime::fromTime_t(ct->phoneNumbers().lastUsedTimeStamp()).toString();
         break;
      default:
         cat = ct->formattedName();
   }
   if (cat.size() && !m_ShowAll)
      cat = cat[0].toUpper();
   return cat;
}

void ContactProxyModel::setRole(int role)
{
   if (role != m_Role) {
      m_Role = role;
      reloadCategories();
   }
}

void ContactProxyModel::setShowAll(bool showAll)
{
   if (showAll != m_ShowAll) {
      m_ShowAll = showAll;
      reloadCategories();
   }
}
