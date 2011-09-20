#include "AkonadiBackend.h"
#include <QtCore/QTimer>
#include <akonadi/control.h>
#include <akonadi/collectionfilterproxymodel.h>
#include <akonadi/collectionmodel.h>
#include <akonadi/kmime/messagemodel.h>
#include <contactgroupsearchjob.h>

AkonadiBackend::AkonadiBackend(QObject* parent) : QObject(parent)
{
   QTimer::singleShot( 0, this, SLOT( delayedInit() ) );
}

virtual AkonadiBackend::~AkonadiBackend()
{
   
}

AkonadiBackend* AkonadiBackend::getInstance()
{
   
}

static bool AkonadiBackend::init() 
{
   if ( !Akonadi::Control::start( this ) ) {
       return false;
   }
   return true;
}

void AkonadiBackend::createModels()
{
    Akonadi::CollectionModel *collectionModel = new Akonadi::CollectionModel( this );
 
    Akonadi::CollectionFilterProxyModel *filterModel = new Akonadi::CollectionFilterProxyModel( this );
    filterModel->setSourceModel( collectionModel );
    filterModel->addMimeTypeFilter( QLatin1String( "message/rfc822" ) );
 
    Akonadi::ItemModel *itemModel = new Akonadi::MessageModel( this );
 
    ui_detacherview_base.folderView->setModel( filterModel );
    ui_detacherview_base.messageView->setModel( itemModel );
 
    connect( ui_detacherview_base.folderView, SIGNAL( currentChanged( Akonadi::Collection ) ),
             itemModel, SLOT( setCollection( Akonadi::Collection ) ) );
}