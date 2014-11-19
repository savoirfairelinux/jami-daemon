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
#ifndef CONTACT_PROXY_MODEL_H
#define CONTACT_PROXY_MODEL_H

#include <QtCore/QHash>
#include <QtCore/QStringList>
#include <QtCore/QAbstractItemModel>

//SFLPhone
#include "typedefs.h"
#include "contact.h"
class ContactModel;
class ContactTreeNode;
class TopLevelItem;
class ContactTreeBinder;

class LIB_EXPORT ContactProxyModel :  public QAbstractItemModel
{
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
public:
   friend class ContactModel;
   friend class ContactTreeNode;
   friend class ContactTreeBinder;
   explicit ContactProxyModel(int role = Qt::DisplayRole, bool showAll = false);
   virtual ~ContactProxyModel();

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
   virtual bool dropMimeData         ( const QMimeData*, Qt::DropAction, int, int, const QModelIndex& );

   //Getter
   static int acceptedPayloadTypes();

private:

   //Helpers
   QString category(const Contact* ct) const;

   //Attributes
   QHash<Contact*, time_t>      m_hContactByDate   ;
   QVector<TopLevelItem*>       m_lCategoryCounter ;
   QHash<QString,TopLevelItem*> m_hCategories      ;
   int                          m_Role             ;
   bool                         m_ShowAll          ;
   QStringList                  m_lMimes           ;

   //Helper
   TopLevelItem* getTopLevelItem(const QString& category);

private Q_SLOTS:
   void reloadCategories();
   void slotContactAdded(Contact* c);
};

class ContactTreeBinder : public QObject { //FIXME Qt5 remove when dropping Qt4
   Q_OBJECT
public:
   ContactTreeBinder(ContactProxyModel* m,ContactTreeNode* n);
private:
   ContactTreeNode* m_pTreeNode;
   ContactProxyModel* m_pModel;
private Q_SLOTS:
   void slotContactChanged();
   void slotStatusChanged();
   void slotPhoneNumberCountChanged(int,int);
   void slotPhoneNumberCountAboutToChange(int,int);
};

#endif
