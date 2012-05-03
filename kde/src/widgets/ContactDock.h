/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
 *   Author : Emmanuel Lepage Valle <emmanuel.lepage@savoirfairelinux.com >*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 **************************************************************************/
#ifndef CONTACT_DOCK_H
#define CONTACT_DOCK_H

#include <QtCore/QHash>
#include <QtGui/QDockWidget>
#include "CategorizedTreeWidget.h"
#include "SortableDockCommon.h"

//Qt
class QSplitter;
class QListWidget;
class QComboBox;
class QTreeWidgetItem;
class QCheckBox;
class QStringList;
class DateTime;

//KDE
class KLineEdit;

namespace Akonadi {
   class EntityTreeView;
   class ItemView;
   class CollectionCombobox;
   namespace Contact {
      class ContactsTreeModel;
   }
}

namespace KABC {
   class Addressee;
}

///SFLPhone
class ContactTree;
class ContactItemWidget;
class StaticEventHandler;
class Contact;

///@class ContactDock Dock to access contacts
class ContactDock : public QDockWidget, public SortableDockCommon
{
   Q_OBJECT
public:
   //Constructor
   ContactDock(QWidget* parent);
   virtual ~ContactDock();

private:
   //Attributes
   KLineEdit*                   m_pFilterLE   ;
   QSplitter*                   m_pSplitter   ;
   ContactTree*                 m_pContactView;
   QListWidget*                 m_pCallView   ;
   QComboBox*                   m_pSortByCBB  ;
   QCheckBox*                   m_pShowHistoCK;
   QList<ContactItemWidget*>    m_Contacts;

public slots:
   virtual void keyPressEvent(QKeyEvent* event);

private slots:
   void reloadContact      (                       );
   void loadContactHistory ( QTreeWidgetItem* item );
   void filter             ( const QString& text   );
   void setHistoryVisible  ( bool visible          );
   void reloadHistoryConst (                       );
};

///@class ContactTree tree view with additinal drag and drop
class ContactTree : public CategorizedTreeWidget {
   Q_OBJECT
public:
   ContactTree(QWidget* parent) : CategorizedTreeWidget(parent) {}
   virtual QMimeData* mimeData( const QList<QTreeWidgetItem *> items) const;
   bool dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);
};

///@class KeyPressEaterC keygrabber
class KeyPressEaterC : public QObject
{
   Q_OBJECT
public:
   KeyPressEaterC(ContactDock* parent) : QObject(parent) {
      m_pDock =  parent;
   }

protected:
   bool eventFilter(QObject *obj, QEvent *event);

private:
   ContactDock* m_pDock;

};

#endif