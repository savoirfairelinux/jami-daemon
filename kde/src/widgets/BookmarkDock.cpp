/***************************************************************************
 *   Copyright (C) 2009-2010 by Savoir-Faire Linux                         *
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

//Parent
#include "BookmarkDock.h"

//Qt
#include <QtGui/QVBoxLayout>
#include <QtGui/QTreeWidgetItem>
#include <QtGui/QTreeWidget>
#include <QtGui/QSplitter>

//KDE
#include <KLocalizedString>
#include <KIcon>
#include <KLineEdit>

//SFLPhone
#include "conf/ConfigurationSkeleton.h"
#include "widgets/HistoryTreeItem.h"

///@class QNumericTreeWidgetItem : Tree widget with different sorting criterias
class QNumericTreeWidgetItem : public QTreeWidgetItem {
   public:
      QNumericTreeWidgetItem(QTreeWidget* parent):QTreeWidgetItem(parent),widget(0),weight(-1){}
      QNumericTreeWidgetItem(QTreeWidgetItem* parent):QTreeWidgetItem(parent),widget(0),weight(-1){}
      HistoryTreeItem* widget;
      int weight;
   private:
      bool operator<(const QTreeWidgetItem & other) const {
         int column = treeWidget()->sortColumn();
         if (dynamic_cast<QNumericTreeWidgetItem*>((QTreeWidgetItem*)&other)) {
            if (widget !=0 && dynamic_cast<QNumericTreeWidgetItem*>((QTreeWidgetItem*)&other)->widget != 0)
               return widget->getTimeStamp() < dynamic_cast<QNumericTreeWidgetItem*>((QTreeWidgetItem*)&other)->widget->getTimeStamp();
            else if (weight > 0 && dynamic_cast<QNumericTreeWidgetItem*>((QTreeWidgetItem*)&other)->weight > 0)
               return weight > dynamic_cast<QNumericTreeWidgetItem*>((QTreeWidgetItem*)&other)->weight;
         }
         return text(column) < other.text(column);
      }
};

///Constructor
BookmarkDock::BookmarkDock(QWidget* parent) : QDockWidget(parent)
{
   setObjectName("bookmarkDock");
   setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
   setMinimumSize(250,0);
   
   m_pFilterLE   = new KLineEdit(this);
   m_pSplitter   = new QSplitter(Qt::Vertical,this);
   m_pItemView   = new QTreeWidget(this);

   QWidget* mainWidget = new QWidget(this);
   setWidget(mainWidget);

   QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

   mainLayout->addWidget(m_pSplitter);
   m_pSplitter->addWidget(m_pItemView);
   mainLayout->addWidget(m_pFilterLE);
   
   m_pSplitter->setChildrenCollapsible(true);
   m_pSplitter->setStretchFactor(0,7);
   
   setWindowTitle(i18n("Bookmark"));
   m_pItemView->headerItem()->setText(0,i18n("Bookmark") );

   foreach (QString nb, ConfigurationSkeleton::bookmarkList()) {
      addBookmark_internal(nb);
   }

   connect(m_pFilterLE, SIGNAL(textChanged(QString)), this, SLOT(filter(QString) ));
}

///Destructor
BookmarkDock::~BookmarkDock()
{
   
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Add a new bookmark
void BookmarkDock::addBookmark_internal(QString phone)
{
   HistoryTreeItem* widget = new HistoryTreeItem(m_pItemView,phone);
   QTreeWidgetItem* item   = new QTreeWidgetItem(m_pItemView      );
   widget->setItem(item);
   m_pItemView->addTopLevelItem(item);
   m_pItemView->setItemWidget(item,0,widget);
   m_pBookmark << widget;
}

///Proxy to add a new bookmark
void BookmarkDock::addBookmark(QString phone)
{
   addBookmark_internal(phone);
   ConfigurationSkeleton::setBookmarkList(ConfigurationSkeleton::bookmarkList() << phone);
}

///Filter the list
void BookmarkDock::filter(QString text)
{
   foreach(HistoryTreeItem* item, m_pBookmark) {
      bool visible = (item->getName().toLower().indexOf(text) != -1) || (item->getPhoneNumber().toLower().indexOf(text) != -1);
      item->getItem()-> setHidden(!visible);
   }
   m_pItemView->expandAll();
}