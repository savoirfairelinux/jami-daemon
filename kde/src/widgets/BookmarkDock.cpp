#include "BookmarkDock.h"

#include <QtGui/QVBoxLayout>
#include <QtGui/QTreeWidgetItem>
#include <KLocalizedString>
#include <kicon.h>
#include <klineedit.h>
#include <QtGui/QTreeWidget>
#include <QtGui/QSplitter>
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