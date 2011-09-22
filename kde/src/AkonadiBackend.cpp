#include "AkonadiBackend.h"
#include <QtCore/QTimer>
#include <akonadi/control.h>
#include <akonadi/collectionfilterproxymodel.h>
#include <akonadi/collectionmodel.h>
#include <akonadi/kmime/messagemodel.h>
#include <kabc/contactgroup.h>
#include <kabc/phonenumber.h>
#include <akonadi/recursiveitemfetchjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>
#include <akonadi/contact/contacteditor.h>
#include <kdialog.h>

#include "lib/Contact.h"
#include "SFLPhone.h"

AkonadiBackend*  AkonadiBackend::m_pInstance = 0;

AkonadiBackend::AkonadiBackend(QObject* parent) : QObject(parent)
{
   //QTimer::singleShot( 0, this, SLOT( delayedInit() ) );
   m_pSession = new Akonadi::Session( "SFLPhone::instance" );

    // fetching all collections containing emails recursively, starting at the root collection
   Akonadi::CollectionFetchJob *job = new Akonadi::CollectionFetchJob( Akonadi::Collection::root(), Akonadi::CollectionFetchJob::Recursive, this );
   job->fetchScope().setContentMimeTypes( QStringList() << "text/directory" );
   connect( job, SIGNAL( collectionsReceived( const Akonadi::Collection::List& ) ), this, SLOT( collectionsReceived( const Akonadi::Collection::List& ) ) );
}

AkonadiBackend::~AkonadiBackend()
{
   
}

AkonadiBackend* AkonadiBackend::getInstance()
{
   if (m_pInstance == NULL) {
      m_pInstance = new AkonadiBackend(0);
   }
   return m_pInstance;
}

ContactList AkonadiBackend::update(Akonadi::Collection collection)
{
   m_pCollection = collection;
   ContactList contacts;
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
            KABC::Addressee tmp = item.payload<KABC::Addressee>();
            Contact* aContact   = new Contact();
            m_pAddrHash[tmp.uid()] = tmp;
            
            KABC::PhoneNumber::List numbers = tmp.phoneNumbers();
            PhoneNumbers newNumbers;
            foreach (KABC::PhoneNumber number, numbers) {
               newNumbers << new Contact::PhoneNumber(number.number(),number.typeLabel());
               m_pContactByPhone[number.number()] = aContact;
            }
            m_pContactByUid[tmp.uid()] = aContact;
            
            aContact->setNickName       (tmp.nickName()       );
            aContact->setFormattedName  (tmp.formattedName()  );
            aContact->setFirstName      (tmp.givenName()      );
            aContact->setFamilyName     (tmp.familyName()     );
            aContact->setOrganization   (tmp.organization()   );
            aContact->setPreferredEmail (tmp.preferredEmail() );
            aContact->setUid            (tmp.uid()            );
            aContact->setPhoneNumbers   (newNumbers           );
            
            if (!tmp.photo().data().isNull())
               aContact->setPhoto(new QPixmap(QPixmap::fromImage( tmp.photo().data()).scaled(QSize(48,48))));
            else
               aContact->setPhoto(0);
            contacts << aContact;
         }
      }
   }
   return contacts;
}

ContactList AkonadiBackend::update()
{
   return update(m_pCollection);
}


Contact* AkonadiBackend::getContactByPhone(QString phoneNumber)
{
   return m_pContactByPhone[phoneNumber];
}

Contact* AkonadiBackend::getContactByUid(QString uid)
{
   return m_pContactByUid[uid];
}

void AkonadiBackend::collectionsReceived( const Akonadi::Collection::List&  list)
{
   foreach (Akonadi::Collection coll, list) {
      update(coll);
      emit collectionChanged();
   }
}

void AkonadiBackend::editContact(Contact* contact)
{
   KABC::Addressee ct = m_pAddrHash[contact->getUid()];
   if (ct.uid() != contact->getUid()) {
      qDebug() << "Contact not found";
      return;
   }
   Akonadi::ContactEditor *editor = new Akonadi::ContactEditor( Akonadi::ContactEditor::EditMode, SFLPhone::app()->view() );
   Akonadi::Item item;
   item.setPayload<KABC::Addressee>(ct);
   editor->loadContact(item);
   KDialog* dlg = new KDialog(SFLPhone::app()->view());
   dlg->setMainWidget(editor);
   dlg->exec();
}

void AkonadiBackend::addNewContact(Contact* contact)
{
   KABC::Addressee newContact;
   newContact.setNickName       ( contact->getNickName()        );
   newContact.setFormattedName  ( contact->getFormattedName()   );
   newContact.setGivenName      ( contact->getFirstName()       );
   newContact.setFamilyName     ( contact->getSecondName()      );
   newContact.setOrganization   ( contact->getOrganization()    );
   //newContact.setPreferredEmail ( contact->getPreferredEmail()  );//TODO

            
   //aContact->setPhoneNumbers   (newNumbers           );//TODO
   
    Akonadi::ContactEditor *editor = new Akonadi::ContactEditor( Akonadi::ContactEditor::CreateMode, SFLPhone::app()->view() );

   editor->setContactTemplate(newContact);
   
   KDialog* dlg = new KDialog(SFLPhone::app()->view());
   dlg->setMainWidget(editor);
   dlg->exec();
   
    
    
   if ( !editor->saveContact() ) {
      qDebug() << "Unable to save new contact to storage";
      return;
   }
}