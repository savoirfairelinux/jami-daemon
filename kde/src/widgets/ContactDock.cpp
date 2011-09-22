
#include "ContactDock.h"

#include <QtGui/QVBoxLayout>
#include <QtGui/QListWidget>
#include <QtGui/QTreeWidget>
#include <QtGui/QHeaderView>
#include <QtGui/QCheckBox>
#include <QtCore/QDateTime>

#include <akonadi/collectionfilterproxymodel.h>
#include <akonadi/contact/contactstreemodel.h>
#include <akonadi/kmime/messagemodel.h>
#include <akonadi/changerecorder.h>
#include <kabc/addressee.h>
#include <kabc/picture.h>
#include <kabc/phonenumber.h>
#include <kabc/vcard.h>
#include <kabc/addressee.h>
#include <kabc/field.h>
#include <kabc/vcardline.h>
#include <kabc/contactgroup.h>
#include <kabc/phonenumber.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/entitydisplayattribute.h>
#include <akonadi/recursiveitemfetchjob.h>
#include <kicon.h>

#include "AkonadiBackend.h"
#include "ContactItemWidget.h"
#include "conf/ConfigurationSkeleton.h"
#include "lib/Call.h"
#include "SFLPhone.h"

class QNumericTreeWidgetItem : public QTreeWidgetItem {
   public:
      QNumericTreeWidgetItem(QTreeWidget* parent):QTreeWidgetItem(parent),widget(0),weight(-1){}
      QNumericTreeWidgetItem(QTreeWidgetItem* parent):QTreeWidgetItem(parent),widget(0),weight(-1){}
      ContactItemWidget* widget;
      QString number;
      int weight;
   private:
      bool operator<(const QTreeWidgetItem & other) const {
         int column = treeWidget()->sortColumn();
         //if (dynamic_cast<QNumericTreeWidgetItem*>((QTreeWidgetItem*)&other)) {
            //if (widget !=0 && dynamic_cast<QNumericTreeWidgetItem*>((QTreeWidgetItem*)&other)->widget != 0)
            //   return widget->getTimeStamp() < dynamic_cast<QNumericTreeWidgetItem*>((QTreeWidgetItem*)&other)->widget->getTimeStamp();
            //else if (weight > 0 && dynamic_cast<QNumericTreeWidgetItem*>((QTreeWidgetItem*)&other)->weight > 0)
            //   return weight > dynamic_cast<QNumericTreeWidgetItem*>((QTreeWidgetItem*)&other)->weight;
         //}
         return text(column) < other.text(column);
      }
};

ContactDock::ContactDock(QWidget* parent) : QDockWidget(parent)
{
   m_pFilterLE     = new KLineEdit   (                   );
   m_pSplitter     = new QSplitter   ( Qt::Vertical,this );
   m_pSortByCBB    = new QComboBox   ( this              );
   m_pContactView  = new ContactTree ( this              );
   m_pCallView     = new QListWidget ( this              );
   m_pShowHistoCK  = new QCheckBox   ( this              );


   QStringList sortType;
   sortType << "Name" << "Organisation" << "Phone number type" << "Rencently used" << "Group";
   m_pSortByCBB->addItems(sortType);

   QWidget* mainWidget = new QWidget(this);
   setWidget(mainWidget);

   m_pContactView->headerItem()->setText(0,"Contacts");
   m_pContactView->header()->setClickable(true);
   m_pContactView->header()->setSortIndicatorShown(true);
   m_pContactView->setAcceptDrops(true);
   m_pContactView->setDragEnabled(true);

   m_pContactView->setAlternatingRowColors(true);

   m_pFilterLE->setPlaceholderText("Filter");
   m_pFilterLE->setClearButtonShown(true);

   m_pShowHistoCK->setChecked(ConfigurationSkeleton::displayContactCallHistory());
   m_pShowHistoCK->setText("Display history");

   QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

   mainLayout->addWidget  ( m_pSortByCBB   );
   mainLayout->addWidget  ( m_pShowHistoCK );
   mainLayout->addWidget  ( m_pSplitter    );
   m_pSplitter->addWidget ( m_pContactView );
   m_pSplitter->addWidget ( m_pCallView    );
   mainLayout->addWidget  ( m_pFilterLE    );

   m_pSplitter->setChildrenCollapsible(true);
   m_pSplitter->setStretchFactor(0,7);
   
   connect (AkonadiBackend::getInstance(),SIGNAL(collectionChanged()),                                   this,        SLOT(reloadContact()                      ));
   connect (m_pContactView,               SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),this,        SLOT(loadContactHistory(QTreeWidgetItem*) ));
   connect (m_pFilterLE,                  SIGNAL(textChanged(QString)),                                  this,        SLOT(filter(QString)                      ));
   connect (m_pShowHistoCK,               SIGNAL(toggled(bool)),                                         m_pCallView, SLOT(setVisible(bool)                     ));
   setWindowTitle("Contact");
}

ContactDock::~ContactDock()
{

}

void ContactDock::reloadContact()
{
   ContactList list = AkonadiBackend::getInstance()->update();
   foreach (Contact* cont, list) {
      ContactItemWidget* aContact  = new ContactItemWidget(m_pContactView);
      QNumericTreeWidgetItem* item = new QNumericTreeWidgetItem(m_pContactView);
      item->widget = aContact;
      aContact->setItem(item);
      aContact->setContact(cont);

      PhoneNumbers numbers =  aContact->getContact()->getPhoneNumbers();
      qDebug() << "Phone count" << numbers.count();
      if (numbers.count() > 1) {
         foreach (Contact::PhoneNumber* number, numbers) {
            QNumericTreeWidgetItem* item2 = new QNumericTreeWidgetItem(item);
            QLabel* numberL = new QLabel("<b>"+number->getType()+":</b>"+number->getNumber(),this);
            item2->number = number->getNumber();
            m_pContactView->setItemWidget(item2,0,numberL);
         }
      }
      else if (numbers.count() == 1) {
         item->number = numbers[0]->getNumber();
      }

      m_pContactView->addTopLevelItem(item);
      m_pContactView->setItemWidget(item,0,aContact);
      m_pContacts << aContact;
   }
}

void ContactDock::loadContactHistory(QTreeWidgetItem* item)
{
   if (m_pShowHistoCK->isChecked()) {
      m_pCallView->clear();
      if (dynamic_cast<QNumericTreeWidgetItem*>(item) != NULL) {
         QNumericTreeWidgetItem* realItem = dynamic_cast<QNumericTreeWidgetItem*>(item);
         foreach (Call* call, SFLPhone::app()->model()->getHistory()) {
            if (realItem->widget != 0) {
               foreach (Contact::PhoneNumber* number, realItem->widget->getContact()->getPhoneNumbers()) {
                  if (number->getNumber() == call->getPeerPhoneNumber()) {
                     m_pCallView->addItem(QDateTime::fromTime_t(call->getStartTimeStamp().toUInt()).toString());
                  }
               }
            }
         }
      }
   }
}

void ContactDock::filter(QString text)
{
   foreach(ContactItemWidget* item, m_pContacts) {
      bool foundNumber = false;
      foreach (Contact::PhoneNumber* number, item->getContact()->getPhoneNumbers()) {
         foundNumber |= number->getNumber().toLower().indexOf(text) != -1;
      }
      bool visible = (item->getContact()->getFormattedName().toLower().indexOf(text) != -1)
                  || (item->getContact()->getOrganization().toLower().indexOf(text) != -1)
                  || (item->getContact()->getPreferredEmail().toLower().indexOf(text) != -1)
                  || foundNumber;
      item->getItem()->setHidden(!visible);
   }
   m_pContactView->expandAll();
}

QMimeData* ContactTree::mimeData( const QList<QTreeWidgetItem *> items) const
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
         mimeData->setData(MIME_CONTACT, item->widget->getContact()->getUid().toUtf8());
      }
      else if (!item->number.isEmpty()) {
         mimeData->setData(MIME_PHONENUMBER, item->number.toUtf8());
      }
   }
   else {
      qDebug() << "the item is not a call";
   }
   return mimeData;
}

bool ContactTree::dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{
   Q_UNUSED(index)
   Q_UNUSED(action)
   Q_UNUSED(parent)

   QByteArray encodedData = data->data(MIME_CALLID);

   qDebug() << "In history import"<< QString(encodedData);

   return false;
}