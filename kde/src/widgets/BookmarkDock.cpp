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

//Parent
#include "BookmarkDock.h"

//Qt
#include <QtGui/QVBoxLayout>
#include <QtGui/QTreeWidgetItem>
#include <QtGui/QTreeWidget>
#include <QtGui/QSplitter>
#include <QtGui/QCheckBox>
#include <QStandardItemModel>

//KDE
#include <KLocalizedString>
#include <KIcon>
#include <KLineEdit>

//SFLPhone
#include "klib/ConfigurationSkeleton.h"
#include "widgets/HistoryTreeItem.h"
#include "SFLPhone.h"
#include "widgets/CategoryDrawer.h"
#include "widgets/CategorizedTreeWidget.h"
#include "klib/AkonadiBackend.h"
#include "klib/HelperFunctions.h"
#include "lib/HistoryModel.h"

///QNumericTreeWidgetItem : Tree widget with different sorting criterias
class QNumericTreeWidgetItem : public QTreeWidgetItem {
   public:
      QNumericTreeWidgetItem(QTreeWidget* parent):QTreeWidgetItem(parent),widget(0),weight(-1){}
      QNumericTreeWidgetItem(QTreeWidgetItem* parent=0):QTreeWidgetItem(parent),widget(0),weight(-1){}
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
   m_pItemView   = new CategorizedTreeWidget(this);
   m_pMostUsedCK = new QCheckBox(this);

   m_pFilterLE->setPlaceholderText(i18n("Filter"));

   m_pMostUsedCK->setChecked(ConfigurationSkeleton::displayContactCallHistory());
   
   QWidget* mainWidget = new QWidget(this);
   setWidget(mainWidget);

   m_pMostUsedCK->setText(i18n("Show most called contacts"));

   QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

   mainLayout->addWidget(m_pMostUsedCK);
   mainLayout->addWidget(m_pSplitter);
   m_pSplitter->addWidget(m_pItemView);
   mainLayout->addWidget(m_pFilterLE);

   m_pSplitter->setChildrenCollapsible(true);
   m_pSplitter->setStretchFactor(0,7);

   setWindowTitle(i18n("Bookmark"));
   m_pItemView->headerItem()->setText(0,i18n("Bookmark") );

   connect(m_pFilterLE                    , SIGNAL(textChanged(QString)), this , SLOT(filter(QString)  ));
   connect(m_pMostUsedCK                  , SIGNAL(toggled(bool)),        this , SLOT(reload()         ));
   connect(AkonadiBackend::getInstance()  , SIGNAL(collectionChanged()) , this , SLOT(reload()  ));
   reload();
} //BookmarkDock

///Destructor
BookmarkDock::~BookmarkDock()
{
   foreach (HistoryTreeItem* hti,m_pBookmark) {
      delete hti;
   }
   delete m_pItemView  ;
   delete m_pFilterLE  ;
   delete m_pSplitter  ;
   delete m_pMostUsedCK;
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Add a new bookmark
void BookmarkDock::addBookmark_internal(const QString& phone)
{
   HistoryTreeItem* widget = new HistoryTreeItem(m_pItemView,phone,true);
   QTreeWidgetItem* item   = NULL;

   if (widget->getName() == i18n("Unknown") || widget->getName().isEmpty()) {
      item = m_pItemView->addItem<QNumericTreeWidgetItem>(i18n("Unknown"));
   }
   else {
      item = m_pItemView->addItem<QNumericTreeWidgetItem>(QString(widget->getName()[0]));
   }
   
   widget->setItem(item);
   m_pItemView->addTopLevelItem(item);
   m_pItemView->setItemWidget(item,0,widget);
   m_pBookmark << widget;
} //addBookmark_internal

///Proxy to add a new bookmark
void BookmarkDock::addBookmark(const QString& phone)
{
   addBookmark_internal(phone);
   ConfigurationSkeleton::setBookmarkList(ConfigurationSkeleton::bookmarkList() << phone);
}

///Remove a bookmark
void BookmarkDock::removeBookmark(const QString& phone)
{
   foreach (HistoryTreeItem* w,m_pBookmark) {
      if (w->getPhoneNumber() == phone) {
         QTreeWidgetItem* item = w->getItem();
         m_pItemView->removeItem(item);
         QStringList bookmarks = ConfigurationSkeleton::bookmarkList();
         if (bookmarks.indexOf(phone)!= -1) {
            bookmarks.removeAt(bookmarks.indexOf(phone));
            ConfigurationSkeleton::setBookmarkList(bookmarks);
         }
         if (m_pBookmark.indexOf(w)!=-1) {
            m_pBookmark.removeAt(m_pBookmark.indexOf(w));
         }
      }
   }
}

///Filter the list
void BookmarkDock::filter(QString text)
{
   foreach(HistoryTreeItem* item, m_pBookmark) {
      bool visible = (HelperFunctions::normStrippped(item->getName()).indexOf(HelperFunctions::normStrippped(text)) != -1)
         || (HelperFunctions::normStrippped(item->getPhoneNumber()).indexOf(HelperFunctions::normStrippped(text)) != -1);
      item->getItem()->setHidden(!visible);
   }
   m_pItemView->expandAll();
}

///Show the most popular items
void BookmarkDock::reload()
{
   m_pItemView->clear();
   m_pBookmark.clear();
   m_pItemView->addCategory(i18n("Popular"));
   for (int i=65;i<=90;i++) {
      m_pItemView->addCategory(QString(i));
   }
   if (m_pMostUsedCK->isChecked()) {
      QStringList cl = HistoryModel::getNumbersByPopularity();
      for (int i=0;i < ((cl.size() < 10)?cl.size():10);i++) {
         QNumericTreeWidgetItem* item = m_pItemView->addItem<QNumericTreeWidgetItem>(i18n("Popular"));
         HistoryTreeItem* widget = new HistoryTreeItem(m_pItemView,cl[i],true);
         widget->setItem(item);
         m_pItemView->setItemWidget(item,0,widget);
         m_pBookmark << widget;
      }
   }
   foreach (QString nb, ConfigurationSkeleton::bookmarkList()) {
      addBookmark_internal(nb);
   }
   ConfigurationSkeleton::setDisplayContactCallHistory(m_pMostUsedCK->isChecked());
} //reload