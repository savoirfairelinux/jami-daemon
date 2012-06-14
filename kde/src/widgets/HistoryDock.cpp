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
#include "HistoryDock.h"

//Qt
#include <QtCore/QString>
#include <QtCore/QDate>
#include <QtGui/QTreeWidget>
#include <QtGui/QComboBox>
#include <QtGui/QPushButton>
#include <QtGui/QLabel>
#include <QtGui/QTreeWidgetItem>
#include <QtGui/QCheckBox>
#include <QtGui/QGridLayout>
#include <QtGui/QHeaderView>

//KDE
#include <KDebug>
#include <KIcon>
#include <KLineEdit>
#include <KDateWidget>

//SFLPhone
#include "SFLPhone.h"
#include "widgets/HistoryTreeItem.h"
#include "klib/AkonadiBackend.h"
#include "klib/ConfigurationSkeleton.h"
#include "lib/HistoryModel.h"

//SFLPhone library
#include "lib/sflphone_const.h"


#define CURRENT_SORTING_MODE m_pSortByCBB->currentIndex()

///QNumericTreeWidgetItem: Qt lack official functional sorting algo, so this hack around it
class QNumericTreeWidgetItem : public QTreeWidgetItem {
   public:
      QNumericTreeWidgetItem(QTreeWidget* parent=0):QTreeWidgetItem(parent),widget(0),weight(-1){}
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

///Event filter allowing to write text on the Tree widget to filter it.
bool KeyPressEater::eventFilter(QObject *obj, QEvent *event)
{
   if (event->type() == QEvent::KeyPress) {
      m_pDock->keyPressEvent((QKeyEvent*)event);
      return true;
   } else {
      // standard event processing
      return QObject::eventFilter(obj, event);
   }
}

///Constructor
HistoryDock::HistoryDock(QWidget* parent) : QDockWidget(parent)
{
   setObjectName("historyDock");
   setMinimumSize(250,0);
   setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
   m_pFilterLE   = new KLineEdit   (                    );
   m_pItemView   = new HistoryTree ( this               );
   m_pSortByCBB  = new QComboBox   (                    );
   m_pSortByL    = new QLabel      ( i18n("Sort by:")   );
   m_pFromL      = new QLabel      ( i18n("From:")      );
   m_pToL        = new QLabel      ( i18n("To:")        );
   m_pFromDW     = new KDateWidget (                    );
   m_pToDW       = new KDateWidget (                    );
   m_pAllTimeCB  = new QCheckBox   ( i18n("Display all"));
   m_pLinkPB     = new QPushButton ( this               );

   m_pAllTimeCB->setChecked(!ConfigurationSkeleton::displayDataRange());
   enableDateRange(!ConfigurationSkeleton::displayDataRange());

   m_pSortByL->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Preferred);
   m_pSortByCBB->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
   m_pLinkPB->setMaximumSize(20,9999999);
   m_pLinkPB->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Expanding);
   m_pLinkPB->setCheckable(true);

   m_pItemView->headerItem()->setText(0,i18n("Calls")   );
   m_pItemView->header    ()->setClickable(true          );
   m_pItemView->header    ()->setSortIndicatorShown(true );
   //m_pItemView->setAlternatingRowColors(true             );
   m_pItemView->setAcceptDrops( true                     );
   m_pItemView->setDragEnabled( true                     );
   m_pKeyPressEater = new KeyPressEater(this);
   m_pItemView->installEventFilter(m_pKeyPressEater);

   m_pFilterLE->setPlaceholderText(i18n("Filter"));
   m_pFilterLE->setClearButtonShown(true);

   QStringList sortBy;
   sortBy << i18n("Date") << i18n("Name") << i18n("Popularity") << i18n("Length");
   m_pSortByCBB->addItems(sortBy);

   QWidget* mainWidget = new QWidget(this);
   setWidget(mainWidget);

   QGridLayout* mainLayout = new QGridLayout(mainWidget);

   mainLayout->addWidget(m_pSortByL   ,0,0     );
   mainLayout->addWidget(m_pSortByCBB ,0,1,1,2 );
   mainLayout->addWidget(m_pAllTimeCB ,1,0,1,3 );
   mainLayout->addWidget(m_pLinkPB    ,3,2,3,1 );
   mainLayout->addWidget(m_pFromL     ,2,0,1,2 );
   mainLayout->addWidget(m_pFromDW    ,3,0,1,2 );
   mainLayout->addWidget(m_pToL       ,4,0,1,2 );
   mainLayout->addWidget(m_pToDW      ,5,0,1,2 );
   mainLayout->addWidget(m_pItemView  ,6,0,1,3 );
   mainLayout->addWidget(m_pFilterLE  ,7,0,1,3 );

   setWindowTitle(i18n("History"));

   QDate date(2000,1,1);
   m_pFromDW->setDate(date);

   m_CurrentFromDate = m_pFromDW->date();
   m_CurrentToDate   = m_pToDW->date();
   
   m_pSortByCBB->setCurrentIndex(ConfigurationSkeleton::historySortMode());

   connect(m_pAllTimeCB,                   SIGNAL(toggled(bool)),            this, SLOT(enableDateRange(bool)       ));
   connect(m_pFilterLE,                    SIGNAL(textChanged(QString)),     this, SLOT(filter(QString)             ));
   connect(m_pFromDW  ,                    SIGNAL(changed(QDate)),           this, SLOT(updateLinkedFromDate(QDate) ));
   connect(m_pToDW    ,                    SIGNAL(changed(QDate)),           this, SLOT(updateLinkedToDate(QDate)   ));
   connect(m_pSortByCBB,                   SIGNAL(currentIndexChanged(int)), this, SLOT(reload()                    ));
   connect(AkonadiBackend::getInstance(),  SIGNAL(collectionChanged()),      this, SLOT(updateContactInfo()         ));
   connect(HistoryModel::self()         ,  SIGNAL(newHistoryCall(Call*)),    this, SLOT(newHistoryCall(Call*)       ));

   reload();
} //HistoryDock

///Destructor
HistoryDock::~HistoryDock()
{
   foreach (HistoryTreeItem* w, m_History) {
      delete w;
   }
   m_History.clear();
   delete m_pItemView     ;
   delete m_pFilterLE     ;
   delete m_pSortByCBB    ;
   delete m_pSortByL      ;
   delete m_pFromL        ;
   delete m_pToL          ;
   delete m_pFromDW       ;
   delete m_pToDW         ;
   delete m_pAllTimeCB    ;
   delete m_pLinkPB       ;
   delete m_pKeyPressEater;
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Update informations
void HistoryDock::updateContactInfo()
{
   foreach(HistoryTreeItem* hitem, m_History) {
      hitem->updated();
   }
}

///Reload the history list
void HistoryDock::reload()
{
   m_pItemView->clear();

   QHash<Contact*, QDateTime> recentlyUsed;
   switch (CURRENT_SORTING_MODE) {
      case Date:
         foreach (QString cat, m_slHistoryConst) {
            m_pItemView->addCategory(i18n(cat.toAscii()));
         }
         break;
      case Popularity: {
         m_hGroup.clear();
         break;
      }
   }
   
   foreach(HistoryTreeItem* hitem, m_History) {
      delete hitem;
   }
   m_History.clear();
   foreach (Call* call, HistoryModel::getHistory()) {
      newHistoryCall(call);
   }
   
   ConfigurationSkeleton::setHistorySortMode(CURRENT_SORTING_MODE);

   switch (CURRENT_SORTING_MODE) {
      case Date:
         for (int i=0;i<m_pItemView->topLevelItemCount();i++) {
            m_pItemView->topLevelItem(i)->sortChildren(0,Qt::DescendingOrder);
         }
         break;
      case Popularity: {
         QMutableHashIterator<QString,QNumericTreeWidgetItem*> iter(m_hGroup);
         while (iter.hasNext()) {
            iter.next();
            QNumericTreeWidgetItem* item = iter.value();
            item->setText(0,iter.key()+" ("+QString::number(item->weight)+")");
         }
         break;
      }
      default:
         m_pItemView->sortItems(0,Qt::AscendingOrder);
   }
} //reload

///Faster way to sync the history than reloading it (2+ seconds)
void HistoryDock::newHistoryCall(Call* call)
{
   HistoryTreeItem* callItem = nullptr;
   if (call != nullptr && (!m_pAllTimeCB->isChecked() || (QDateTime(m_pFromDW->date()).toTime_t() < call->getStartTimeStamp().toUInt() && QDateTime(m_pToDW->date().addDays(1)).toTime_t() > call->getStartTimeStamp().toUInt() ))) {
      callItem = new HistoryTreeItem(m_pItemView);
      callItem->setCall(call);
      m_History << callItem;
   }
   else
      return;
   switch (CURRENT_SORTING_MODE) {
      case Date: {
         QString category = timeToHistoryCategory(QDateTime::fromTime_t(callItem->call()->getStartTimeStamp().toUInt()).date());
         QNumericTreeWidgetItem* item = m_pItemView->addItem<QNumericTreeWidgetItem>(category);
         item->weight = -callItem->call()->getStopTimeStamp().toUInt();
         item->widget = callItem;
         callItem->setItem(item);
         m_pItemView->setItemWidget(item,0,callItem);
         break;
      }
      case Name2: {
         QNumericTreeWidgetItem* twItem = m_pItemView->addItem<QNumericTreeWidgetItem>(callItem->getName());
         callItem->setItem(twItem);
         twItem->widget = callItem;
         m_pItemView->setItemWidget(twItem,0,callItem);
         break;
      }
      case Popularity: {
         QString name = callItem->getName().trimmed();
         if (!m_hGroup[name]) {
            m_hGroup[name] = m_pItemView->addCategory<QNumericTreeWidgetItem>(name);
            m_hGroup[name]->weight = 0;
            m_pItemView->addTopLevelItem(m_hGroup[name]);
         }
         m_hGroup[name]->weight++;
         QNumericTreeWidgetItem* twItem = m_pItemView->addItem<QNumericTreeWidgetItem>(name);
         callItem->setItem(twItem);
         twItem->widget = callItem;
         m_pItemView->setItemWidget(twItem,0,callItem);
         break;
      }
      case Length: {
         QNumericTreeWidgetItem* item = m_pItemView->addItem<QNumericTreeWidgetItem>(" ");
         item->weight = callItem->getLength();
         callItem->setItem(item);
         m_pItemView->addTopLevelItem(item);
         m_pItemView->setItemWidget(item,0,callItem);
         break;
      }
   }
} //newHistoryCall

///Enable the ability to set a date range like 1 month to limit history
void HistoryDock::enableDateRange(bool disable)
{
   m_pFromL->setVisible (!disable);
   m_pToL->setVisible   (!disable);
   m_pFromDW->setVisible(!disable);
   m_pToDW->setVisible  (!disable);
   m_pLinkPB->setVisible(!disable);

   ConfigurationSkeleton::setDisplayDataRange(!disable);
}

///Filter the history
void HistoryDock::filter(QString text)
{
   QString lower = text.toLower();
   foreach(HistoryTreeItem* item, m_History) {
      bool visible = ( HelperFunctions::normStrippped( item->getName()        ).indexOf( lower ) != -1)
                  || ( HelperFunctions::normStrippped( item->getPhoneNumber() ).indexOf( lower ) != -1);
      item->getItem()-> setHidden(!visible);
   }
   m_pItemView->expandAll();
}

///When the data range is linked, change the opposite value when editing the first
void HistoryDock::updateLinkedDate(KDateWidget* item, QDate& prevDate, QDate& newDate)
{
   if (m_pLinkPB->isChecked()) {
      if (prevDate.day() != newDate.day()) {
         QDate tmp = item->date();
         tmp = tmp.addDays(newDate.day() - prevDate.day());
         item->setDate(tmp);
      }
      if (prevDate.month() != newDate.month()) {
         QDate tmp = item->date();
         tmp = tmp.addMonths(newDate.month() - prevDate.month());
         item->setDate(tmp);
      }
      if (prevDate.year() != newDate.year()) {
         QDate tmp = item->date();
         tmp = tmp.addYears(newDate.year() - prevDate.year());
         item->setDate(tmp);
      }
   }
   prevDate = newDate;
} //updateLinkedDate

///The signals have to be disabled to prevent an ifinite loop
void HistoryDock::updateLinkedFromDate(QDate date)
{
   disconnect (m_pToDW  ,  SIGNAL(changed(QDate)),       this, SLOT(updateLinkedToDate(QDate)));
   updateLinkedDate(m_pToDW,m_CurrentFromDate,date);
   connect    (m_pToDW  ,  SIGNAL(changed(QDate)),       this, SLOT(updateLinkedToDate(QDate)));
}

///The signals have to be disabled to prevent an ifinite loop
void HistoryDock::updateLinkedToDate(QDate date)
{
   disconnect(m_pFromDW  ,  SIGNAL(changed(QDate)),       this, SLOT(updateLinkedFromDate(QDate)));
   updateLinkedDate(m_pFromDW,m_CurrentToDate,date);
   connect   (m_pFromDW  ,  SIGNAL(changed(QDate)),       this, SLOT(updateLinkedFromDate(QDate)));
}


/*****************************************************************************
 *                                                                           *
 *                             Drag and drop                                 *
 *                                                                           *
 ****************************************************************************/

///Generate serializerd version of the content
QMimeData* HistoryTree::mimeData( const QList<QTreeWidgetItem *> items) const
{
   kDebug() << "An history call is being dragged";
   if (items.size() < 1) {
      return NULL;
   }

   QMimeData *mimeData = new QMimeData();

   //Contact
   if (dynamic_cast<QNumericTreeWidgetItem*>(items[0])) {
      QNumericTreeWidgetItem* item = dynamic_cast<QNumericTreeWidgetItem*>(items[0]);
      if (item->widget != 0) {
         mimeData->setData(MIME_PHONENUMBER, item->widget->call()->getPeerPhoneNumber().toUtf8());
      }
   }
   else {
      kDebug() << "the item is not a call";
   }
   return mimeData;
} //mimeData

///Handle what happen when serialized data is dropped
bool HistoryTree::dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{
   Q_UNUSED( index  )
   Q_UNUSED( action )
   Q_UNUSED( parent )

   QByteArray encodedData = data->data(MIME_CALLID);

   kDebug() << "In history import"<< QString(encodedData);

   return false;
}


/*****************************************************************************
 *                                                                           *
 *                              Keyboard handling                            *
 *                                                                           *
 ****************************************************************************/

///Handle keyboard input and redirect them to the filterbox
void HistoryDock::keyPressEvent(QKeyEvent* event) {
   int key = event->key();
   if(key == Qt::Key_Escape)
      m_pFilterLE->setText(QString());
   else if ((key == Qt::Key_Return || key == Qt::Key_Enter) && m_pItemView->selectedItems().size() > 0) {
      if (m_pItemView->selectedItems()[0] && m_pItemView->itemWidget(m_pItemView->selectedItems()[0],0)) {
         QNumericTreeWidgetItem* item = dynamic_cast<QNumericTreeWidgetItem*>(m_pItemView->selectedItems()[0]);
         if (item) {
            SFLPhone::model()->addDialingCall(item->widget->getName(), SFLPhone::app()->model()->getCurrentAccount())->setCallNumber(item->widget->getPhoneNumber());
         }
      }
   }
   else if((key == Qt::Key_Backspace) && (m_pFilterLE->text().size()))
      m_pFilterLE->setText(m_pFilterLE->text().left( m_pFilterLE->text().size()-1 ));
   else if (!event->text().isEmpty() && !(key == Qt::Key_Backspace))
      m_pFilterLE->setText(m_pFilterLE->text()+event->text());
} //keyPressEvent