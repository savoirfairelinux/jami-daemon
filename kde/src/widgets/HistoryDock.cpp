#include "HistoryDock.h"

#include <kicon.h>
#include <klineedit.h>
#include <QtGui/QTreeWidget>
#include <QtGui/QComboBox>
#include <QtGui/QPushButton>
#include <QtGui/QLabel>
#include <QDebug>
#include <QtGui/QTreeWidgetItem>
#include <QtCore/QString>
#include <kdatewidget.h>
#include <QHeaderView>
#include <QtGui/QCheckBox>
#include <QtGui/QGridLayout>
#include <QDate>
#include "SFLPhone.h"
#include "widgets/HistoryTreeItem.h"
#include "conf/ConfigurationSkeleton.h"
#include "AkonadiBackend.h"
#include "lib/sflphone_const.h"

///Qt lack official functional sorting algo, so this hack around it
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
   m_pFilterLE   = new KLineEdit();
   m_pItemView   = new HistoryTree(this);
   m_pSortByCBB  = new QComboBox();
   m_pSortByL    = new QLabel(i18n("Sort by:"));
   m_pFromL      = new QLabel(i18n("From:"));
   m_pToL        = new QLabel(i18n("To:"));
   m_pFromDW     = new KDateWidget();
   m_pToDW       = new KDateWidget();
   m_pAllTimeCB  = new QCheckBox(i18n("Display all"));
   m_pLinkPB     = new QPushButton(this);
   
   m_pAllTimeCB->setChecked(ConfigurationSkeleton::displayDataRange());
   enableDateRange(ConfigurationSkeleton::displayDataRange());

   m_pSortByL->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Preferred);
   m_pSortByCBB->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
   m_pLinkPB->setMaximumSize(20,9999999);
   m_pLinkPB->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Expanding);
   m_pLinkPB->setCheckable(true);
   
   m_pItemView->headerItem()->setText(0,i18n("Calls")   );
   m_pItemView->header    ()->setClickable(true          );
   m_pItemView->header    ()->setSortIndicatorShown(true );
   m_pItemView->setAlternatingRowColors(true             );
   m_pItemView->setAcceptDrops( true                     );
   m_pItemView->setDragEnabled( true                     );
   KeyPressEater *keyPressEater = new KeyPressEater(this);
   m_pItemView->installEventFilter(keyPressEater);

   m_pFilterLE->setPlaceholderText(i18n("Filter"));
   m_pFilterLE->setClearButtonShown(true);
   
   QStringList sortBy;
   sortBy << "Date" << "Name" << "Popularity" << "Duration";
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
   
   reload();
   m_pCurrentFromDate = m_pFromDW->date();
   m_pCurrentToDate   = m_pToDW->date();

   connect(m_pAllTimeCB,                  SIGNAL(toggled(bool)),            this, SLOT(enableDateRange(bool)       ));
   connect(m_pFilterLE,                   SIGNAL(textChanged(QString)),     this, SLOT(filter(QString)             ));
   connect(m_pFromDW  ,                   SIGNAL(changed(QDate)),           this, SLOT(updateLinkedFromDate(QDate) ));
   connect(m_pToDW    ,                   SIGNAL(changed(QDate)),           this, SLOT(updateLinkedToDate(QDate)   ));
   connect(m_pSortByCBB,                  SIGNAL(currentIndexChanged(int)), this, SLOT(reload()                    ));
   connect(AkonadiBackend::getInstance(), SIGNAL(collectionChanged()),      this, SLOT(updateContactInfo()         ));
}

///Destructor
HistoryDock::~HistoryDock()
{
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///Return the identity of the call caller
QString HistoryDock::getIdentity(HistoryTreeItem* item)
{
   if (item->getName().trimmed().isEmpty())
      return item->getPhoneNumber();
   else
      return item->getName();
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Update informations
void HistoryDock::updateContactInfo()
{
   foreach(HistoryTreeItem* hitem, m_pHistory) {
      hitem->updated();
   }
}

///Reload the history list
void HistoryDock::reload()
{
   m_pItemView->clear();
   foreach(HistoryTreeItem* hitem, m_pHistory) {
      delete hitem;
   }
   m_pHistory.clear();
   foreach (Call* call, SFLPhone::app()->model()->getHistory()) {
      if (!m_pAllTimeCB->isChecked() || (QDateTime(m_pFromDW->date()).toTime_t() < call->getStartTimeStamp().toUInt() && QDateTime(m_pToDW->date().addDays(1)).toTime_t() > call->getStartTimeStamp().toUInt() )) {
         HistoryTreeItem* callItem = new HistoryTreeItem(m_pItemView);
         callItem->setCall(call);
         m_pHistory << callItem;
      }
   }
   switch (m_pSortByCBB->currentIndex()) {
      case Date:
         foreach(HistoryTreeItem* hitem, m_pHistory) {
            QNumericTreeWidgetItem* item = new QNumericTreeWidgetItem(m_pItemView);
            item->widget = hitem;
            hitem->setItem(item);
            m_pItemView->addTopLevelItem(item);
            m_pItemView->setItemWidget(item,0,hitem);
         }
         break;
      case Name: {
         QHash<QString,QTreeWidgetItem*> group;
         foreach(HistoryTreeItem* item, m_pHistory) {
            if (!group[getIdentity(item)]) {
               group[getIdentity(item)] = new QTreeWidgetItem(m_pItemView);
               group[getIdentity(item)]->setText(0,getIdentity(item));
               m_pItemView->addTopLevelItem(group[getIdentity(item)]);
            }
            QNumericTreeWidgetItem* twItem = new QNumericTreeWidgetItem(group[getIdentity(item)]);
            item->setItem(twItem);
            twItem->widget = item;
            m_pItemView->setItemWidget(twItem,0,item);
         }
         break;
      }
      case Popularity: {
         QHash<QString,QNumericTreeWidgetItem*> group;
         foreach(HistoryTreeItem* item, m_pHistory) {
            if (!group[getIdentity(item)]) {
               group[getIdentity(item)] = new QNumericTreeWidgetItem(m_pItemView);
               group[getIdentity(item)]->weight = 0;
               m_pItemView->addTopLevelItem(group[getIdentity(item)]);
            }
            group[getIdentity(item)]->weight++;
            group[getIdentity(item)]->setText(0,getIdentity(item)+" ("+QString::number(group[getIdentity(item)]->weight)+")");
            QNumericTreeWidgetItem* twItem = new QNumericTreeWidgetItem(group[getIdentity(item)]);
            item->setItem(twItem);
            twItem->widget = item;
            m_pItemView->setItemWidget(twItem,0,item);
         }
         break;
      }
      case Duration:
         foreach(HistoryTreeItem* hitem, m_pHistory) {
            QNumericTreeWidgetItem* item = new QNumericTreeWidgetItem(m_pItemView);
            item->weight = hitem->getDuration();
            hitem->setItem(item);
            m_pItemView->addTopLevelItem(item);
            m_pItemView->setItemWidget(item,0,hitem);
         }
         break;
   }
   m_pItemView->sortItems(0,Qt::AscendingOrder);
}

///Enable the ability to set a date range like 1 month to limit history
void HistoryDock::enableDateRange(bool enable)
{
   m_pFromL->setVisible(enable);
   m_pToL->setVisible(enable);
   m_pFromDW->setVisible(enable);
   m_pToDW->setVisible(enable);
   m_pLinkPB->setVisible(enable);
   
   ConfigurationSkeleton::setDisplayDataRange(enable);
}

///Filter the history
void HistoryDock::filter(QString text)
{
   foreach(HistoryTreeItem* item, m_pHistory) {
      bool visible = (item->getName().toLower().indexOf(text) != -1) || (item->getPhoneNumber().toLower().indexOf(text) != -1);
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
}

///The signals have to be disabled to prevent an ifinite loop
void HistoryDock::updateLinkedFromDate(QDate date)
{
   disconnect (m_pToDW  ,  SIGNAL(changed(QDate)),       this, SLOT(updateLinkedToDate(QDate)));
   updateLinkedDate(m_pToDW,m_pCurrentFromDate,date);
   connect    (m_pToDW  ,  SIGNAL(changed(QDate)),       this, SLOT(updateLinkedToDate(QDate)));
}

///The signals have to be disabled to prevent an ifinite loop
void HistoryDock::updateLinkedToDate(QDate date)
{
   disconnect(m_pFromDW  ,  SIGNAL(changed(QDate)),       this, SLOT(updateLinkedFromDate(QDate)));
   updateLinkedDate(m_pFromDW,m_pCurrentToDate,date);
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
   qDebug() << "An history call is being dragged";
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
      qDebug() << "the item is not a call";
   }
   return mimeData;
}

///Handle what happen when serialized data is dropped
bool HistoryTree::dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{
   Q_UNUSED(index)
   Q_UNUSED(action)
   Q_UNUSED(parent)

   QByteArray encodedData = data->data(MIME_CALLID);

   qDebug() << "In history import"<< QString(encodedData);

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
   else if(key == Qt::Key_Return || key == Qt::Key_Enter) {}
   else if((key == Qt::Key_Backspace) && (m_pFilterLE->text().size()))
      m_pFilterLE->setText(m_pFilterLE->text().left( m_pFilterLE->text().size()-1 ));
   else if (!event->text().isEmpty() && !(key == Qt::Key_Backspace))
      m_pFilterLE->setText(m_pFilterLE->text()+event->text());
}