#include "ContactDock.h"

#include <QtGui/QVBoxLayout>

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
#include <akonadi/itemfetchscope.h>
#include <akonadi/entitydisplayattribute.h>
#include <akonadi/recursiveitemfetchjob.h>
#include <kicon.h>

ContactDock::ContactDock(QWidget* parent) : QDockWidget(parent)
{
   m_pCollViewCV = new Akonadi::EntityTreeView();
   m_pItemView   = new Akonadi::ItemView();
   m_pFilterLE   = new KLineEdit();
   m_pCollCCB    = new Akonadi::CollectionComboBox;
   m_pSplitter   = new QSplitter(Qt::Vertical,this);

   QWidget* mainWidget = new QWidget(this);
   setWidget(mainWidget);

   QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

   mainLayout->addWidget(m_pCollCCB);
   mainLayout->addWidget(m_pSplitter);
   m_pSplitter->addWidget(m_pCollViewCV);
   m_pSplitter->addWidget(m_pItemView);
   mainLayout->addWidget(m_pFilterLE);
   
   m_pSplitter->setChildrenCollapsible(true);
   m_pSplitter->setStretchFactor(0,7);
   
   m_pCollCCB->setMimeTypeFilter( QStringList() << KABC::Addressee::mimeType() );
   m_pCollCCB->setAccessRightsFilter( Akonadi::Collection::ReadOnly );


      /////////////

      // use a separated session for this model
   Akonadi::Session *session = new Akonadi::Session( "SFLPhone::instance" );

   Akonadi::ItemFetchScope scope;
   // fetch all content of the contacts, including images
   scope.fetchFullPayload( true );
   // fetch the EntityDisplayAttribute, which contains custom names and icons
   scope.fetchAttribute<Akonadi::EntityDisplayAttribute>();

   Akonadi::ChangeRecorder *changeRecorder = new Akonadi::ChangeRecorder;
   changeRecorder->setSession( session );
   // include fetching the collection tree
   changeRecorder->fetchCollection( true );
   // set the fetch scope that shall be used
   changeRecorder->setItemFetchScope( scope );
   // monitor all collections below the root collection for changes
   changeRecorder->setCollectionMonitored( Akonadi::Collection::root() );
   // list only contacts and contact groups
   changeRecorder->setMimeTypeMonitored( KABC::Addressee::mimeType(), true );
   changeRecorder->setMimeTypeMonitored( KABC::ContactGroup::mimeType(), true );

   Akonadi::ContactsTreeModel *model = new Akonadi::ContactsTreeModel( changeRecorder );

   Akonadi::ContactsTreeModel::Columns columns;
   columns << Akonadi::ContactsTreeModel::FullName;
   columns << Akonadi::ContactsTreeModel::AllEmails;
   model->setColumns( columns );

   //////////////

   for (int i=0;i<model->rowCount();i++) {
       qDebug() << "\n\n\nChild:" << model->hasChildren(model->index(i,0)) << "Model:" << model->rowCount(model->index(i,0));
   }
   qDebug() << "\n\n\nChild:" << model->hasChildren(model->index(0,0)) << "Model:" << model->rowCount(model->index(0,0));
   qDebug() << "\n\n\nChild:" << model->hasChildren(model->index(1,0)) << "Model:" << model->rowCount(model->index(1,0));

      //qDebug() << "Model:" << *model;

      m_pCollViewCV->setModel( model );
      m_pItemView->setModel( model );
      
      connect (m_pCollCCB, SIGNAL(currentChanged(Akonadi::Collection)),this,SLOT(collectAddressBookContacts()));
      
      
   //collectAllContacts(model);
   collectAddressBookContacts();
   
   setWindowTitle("Contact");
   //setDockIcon(KIcon("resource-group"));
}

ContactDock::~ContactDock()
{
   
}

KABC::Addressee::List ContactDock::collectAllContacts(Akonadi::ContactsTreeModel *mModel) const
{
  KABC::Addressee::List contacts;
  for ( int i = 0; i < mModel->rowCount(); ++i ) {
    const QModelIndex index = mModel->index( i, 0 );
    if ( index.isValid() ) {
      const Akonadi::Item item = index.data( Akonadi::EntityTreeModel::ItemRole ).value<Akonadi::Item>();
      if ( item.isValid() && item.hasPayload<KABC::Addressee>() ) {
        contacts.append( item.payload<KABC::Addressee>() );
	qDebug() << item.payload<KABC::Addressee>().toString();
      }
    }
  }
  return contacts;
}

KABC::Addressee::List ContactDock::collectAddressBookContacts() const
{
   
   qDebug() << "In colect\n\n\n\n\n";
  KABC::Addressee::List contacts;

  
  const Akonadi::Collection collection = m_pCollCCB->currentCollection();
  if ( !collection.isValid() ) {
     qDebug() << "The current collection is not valid";
    return contacts;
  }
  qDebug() << "Valid collection";

  //if ( mAddressBookSelectionRecursive->isChecked() ) {
    Akonadi::RecursiveItemFetchJob *job = new Akonadi::RecursiveItemFetchJob( collection, QStringList() << KABC::Addressee::mimeType() << KABC::ContactGroup::mimeType());
    job->fetchScope().fetchFullPayload();
//qDebug() << "Begin \n\n\n";
    if ( job->exec() ) {
       
      const Akonadi::Item::List items = job->items();

      foreach ( const Akonadi::Item &item, items ) {
	 //qDebug() << "In for" << item.payloadData() << item.mimeType();
        if ( item.hasPayload<KABC::ContactGroup>() ) {
          //contacts.append( item.payload<KABC::ContactGroup>() );
	  qDebug() << "Group:" << item.payload<KABC::ContactGroup>().name();
        }
        if ( item.hasPayload<KABC::Addressee>() ) {
          //contacts.append( item.payload<KABC::ContactGroup>() );
	  qDebug() << "Addressee:" << item.payload<KABC::Addressee>().name();
        }
//         if ( item.hasPayload<KABC::Field>() ) {
//           //contacts.append( item.payload<KABC::VCard>() );
// 	  qDebug() << "VCard:" << item.payload<KABC::VCardFormat>().identifiers();
//         }
      }
    }
//   } else {
//     Akonadi::ItemFetchJob *job = new Akonadi::ItemFetchJob( collection );
//     job->fetchScope().fetchFullPayload();
// 
//     if ( job->exec() ) {
//       const Akonadi::Item::List items = job->items();
// 
//       foreach ( const Akonadi::Item &item, items ) {
//         if ( item.hasPayload<KABC::Addressee>() ) {
//           contacts.append( item.payload<KABC::Addressee>() );
//         }
//       }
//     }
//   }

  qDebug() << "End collect \n\n\n";
  return contacts;
}
