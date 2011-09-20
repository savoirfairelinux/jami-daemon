
#include "ContactDock.h"

#include <QtGui/QVBoxLayout>
#include <QtGui/QTableWidget>
#include <QtGui/QTreeWidget>
#include <QtGui/QHeaderView>

#include <akonadi/collectionfilterproxymodel.h>
#include <akonadi/contact/contactstreemodel.h>
#include <akonadi/kmime/messagemodel.h>
#include <akonadi/changerecorder.h>
#include <akonadi/session.h>
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

#include "ContactItemWidget.h"

class QNumericTreeWidgetItem : public QTreeWidgetItem {
   public:
      QNumericTreeWidgetItem(QTreeWidget* parent):QTreeWidgetItem(parent),widget(0),weight(-1){}
      QNumericTreeWidgetItem(QTreeWidgetItem* parent):QTreeWidgetItem(parent),widget(0),weight(-1){}
      ContactItemWidget* widget;
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
   m_pFilterLE     = new KLineEdit();
   m_pCollCCB      = new Akonadi::CollectionComboBox;
   m_pSplitter     = new QSplitter(Qt::Vertical,this);
   m_pSortByCBB    = new QComboBox(this);
   m_pContactView  = new QTreeWidget(this);
   m_pCallView     = new QTableWidget(this);

   QStringList sortType;
   sortType << "Name" << "Organisation" << "Phone number type" << "Rencently used" << "Group";
   m_pSortByCBB->addItems(sortType);

   QWidget* mainWidget = new QWidget(this);
   setWidget(mainWidget);

   m_pContactView->headerItem()->setText(0,"Contacts");
   m_pContactView->header()->setClickable(true);
   m_pContactView->header()->setSortIndicatorShown(true);

   m_pContactView->setAlternatingRowColors(true);

   m_pFilterLE->setPlaceholderText("Filter");
   m_pFilterLE->setClearButtonShown(true);

   QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

   mainLayout->addWidget(m_pCollCCB);
   mainLayout->addWidget(m_pSortByCBB);
   mainLayout->addWidget(m_pSplitter);
   m_pSplitter->addWidget(m_pContactView);
   m_pSplitter->addWidget(m_pCallView);
   mainLayout->addWidget(m_pFilterLE);

   m_pSplitter->setChildrenCollapsible(true);
   m_pSplitter->setStretchFactor(0,7);

   m_pCollCCB->setMimeTypeFilter( QStringList() << KABC::Addressee::mimeType() );
   m_pCollCCB->setAccessRightsFilter( Akonadi::Collection::ReadOnly );
   
   Akonadi::Session *session = new Akonadi::Session( "SFLPhone::instance" );

   connect (m_pCollCCB, SIGNAL(currentChanged(Akonadi::Collection)),this,SLOT(reloadContact()));
   setWindowTitle("Contact");
}

ContactDock::~ContactDock()
{

}

KABC::Addressee::List ContactDock::collectAddressBookContacts() const
{
   KABC::Addressee::List contacts;
   const Akonadi::Collection collection = m_pCollCCB->currentCollection();
   if ( !collection.isValid() ) {
      qDebug() << "The current collection is not valid";
      return contacts;
   }

   Akonadi::RecursiveItemFetchJob *job = new Akonadi::RecursiveItemFetchJob( collection, QStringList() << KABC::Addressee::mimeType() << KABC::ContactGroup::mimeType());
   job->fetchScope().fetchFullPayload();
   if ( job->exec() ) {

      const Akonadi::Item::List items = job->items();

      foreach ( const Akonadi::Item &item, items ) {
         if ( item.hasPayload<KABC::ContactGroup>() ) {
            
            qDebug() << "Group:" << item.payload<KABC::ContactGroup>().name();
         }
         if ( item.hasPayload<KABC::Addressee>() ) {
            contacts << item.payload<KABC::Addressee>();
            qDebug() << "Addressee:" << item.payload<KABC::Addressee>().givenName();
         }
      }
   }

   qDebug() << "End collect "<< contacts.size() << "\n\n\n";
   return contacts;
}

void ContactDock::reloadContact()
{
   KABC::Addressee::List list = collectAddressBookContacts();

   qDebug() << "About to display items" << list.size();
   foreach (KABC::Addressee addr, list) {
      qDebug() << "In list:";
      ContactItemWidget* aContact  = new ContactItemWidget(m_pContactView);
      QNumericTreeWidgetItem* item = new QNumericTreeWidgetItem(m_pContactView);
      item->widget = aContact;
      aContact->setItem(item);
      aContact->setContact(addr);

      KABC::PhoneNumber::List numbers =  aContact->getCallNumbers();
      if (aContact->getCallNumbers().count() > 1) {
         foreach (KABC::PhoneNumber number, numbers) {
            QNumericTreeWidgetItem* item2 = new QNumericTreeWidgetItem(item);
            QLabel* numberL = new QLabel("<b>"+number.typeLabel()+":</b>"+number.number());
            m_pContactView->setItemWidget(item2,0,numberL);
         }
      }

      m_pContactView->addTopLevelItem(item);
      m_pContactView->setItemWidget(item,0,aContact);
   }
}