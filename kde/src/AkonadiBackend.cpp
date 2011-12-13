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
#include "AkonadiBackend.h"

//Qt
#include <QtCore/QTimer>
#include <QtCore/QObject>

//KDE
#include <kdialog.h>
#include <akonadi/control.h>
#include <akonadi/collectionfilterproxymodel.h>
#include <akonadi/kmime/messagemodel.h>
#include <akonadi/recursiveitemfetchjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>
#include <akonadi/contact/contacteditor.h>
#include <akonadi/session.h>
#include <kabc/addressee.h>
#include <kabc/addresseelist.h>
#include <kabc/contactgroup.h>
#include <kabc/phonenumber.h>

//SFLPhone library
#include "lib/Contact.h"

//SFLPhone
#include "SFLPhone.h"
#include "SFLPhoneView.h"

///Init static attributes
AkonadiBackend*  AkonadiBackend::m_pInstance = 0;

///Constructor
AkonadiBackend::AkonadiBackend(QObject* parent) : ContactBackend(parent)
{
   //QTimer::singleShot( 0, this, SLOT( delayedInit() ) );
   m_pSession = new Akonadi::Session( "SFLPhone::instance" );

   // fetching all collections containing emails recursively, starting at the root collection
   Akonadi::CollectionFetchJob *job = new Akonadi::CollectionFetchJob( Akonadi::Collection::root(), Akonadi::CollectionFetchJob::Recursive, this );
   job->fetchScope().setContentMimeTypes( QStringList() << "text/directory" );
   connect( job, SIGNAL( collectionsReceived( const Akonadi::Collection::List& ) ), this, SLOT( collectionsReceived( const Akonadi::Collection::List& ) ) );
}

///Destructor
AkonadiBackend::~AkonadiBackend()
{
   
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///Singleton
ContactBackend* AkonadiBackend::getInstance()
{
   if (m_pInstance == NULL) {
      m_pInstance = new AkonadiBackend(0);
   }
   return m_pInstance;
}

///Find contact using a phone number
Contact* AkonadiBackend::getContactByPhone(QString phoneNumber)
{
   return m_ContactByPhone[phoneNumber];
}

///Find contact by UID
Contact* AkonadiBackend::getContactByUid(QString uid)
{
   return m_ContactByUid[uid];
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Update the contact list when a new Akonadi collection is added
ContactList AkonadiBackend::update(Akonadi::Collection collection)
{
   m_Collection = collection;
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
            m_AddrHash[tmp.uid()] = tmp;
            
            KABC::PhoneNumber::List numbers = tmp.phoneNumbers();
            PhoneNumbers newNumbers;
            foreach (KABC::PhoneNumber number, numbers) {
               newNumbers << new Contact::PhoneNumber(number.number(),number.typeLabel());
               m_ContactByPhone[number.number()] = aContact;
            }
            m_ContactByUid[tmp.uid()] = aContact;
            
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

///Edit backend value using an updated frontend contact
void AkonadiBackend::editContact(Contact* contact)
{
   KABC::Addressee ct = m_AddrHash[contact->getUid()];
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

///Add a new contact
void AkonadiBackend::addNewContact(Contact* contact)
{
   KABC::Addressee newContact;
   newContact.setNickName       ( contact->getNickName()        );
   newContact.setFormattedName  ( contact->getFormattedName()   );
   newContact.setGivenName      ( contact->getFirstName()       );
   newContact.setFamilyName     ( contact->getSecondName()      );
   newContact.setOrganization   ( contact->getOrganization()    );
   //newContact.setPreferredEmail ( contact->getPreferredEmail()  );//TODO

   foreach (Contact::PhoneNumber* nb, contact->getPhoneNumbers()) {
      KABC::PhoneNumber pn;
      if (nb->getType()      == "Home"   ) pn.setType(KABC::PhoneNumber::Home  );
      else if (nb->getType() == "Work"   ) pn.setType(KABC::PhoneNumber::Work  );
      else if (nb->getType() == "Msg"    ) pn.setType(KABC::PhoneNumber::Msg   );
      else if (nb->getType() == "Pref"   ) pn.setType(KABC::PhoneNumber::Pref  );
      else if (nb->getType() == "Voice"  ) pn.setType(KABC::PhoneNumber::Voice );
      else if (nb->getType() == "Fax"    ) pn.setType(KABC::PhoneNumber::Fax   );
      else if (nb->getType() == "Cell"   ) pn.setType(KABC::PhoneNumber::Cell  );
      else if (nb->getType() == "Video"  ) pn.setType(KABC::PhoneNumber::Video );
      else if (nb->getType() == "Bbs"    ) pn.setType(KABC::PhoneNumber::Bbs   );
      else if (nb->getType() == "Modem"  ) pn.setType(KABC::PhoneNumber::Modem );
      else if (nb->getType() == "Car"    ) pn.setType(KABC::PhoneNumber::Car   );
      else if (nb->getType() == "Isdn"   ) pn.setType(KABC::PhoneNumber::Isdn  );
      else if (nb->getType() == "Pcs"    ) pn.setType(KABC::PhoneNumber::Pcs   );
      else if (nb->getType() == "Pager"  ) pn.setType(KABC::PhoneNumber::Pager );

      pn.setNumber(nb->getNumber());
      newContact.insertPhoneNumber(pn);
   }

            
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


/*****************************************************************************
 *                                                                           *
 *                                    Slots                                  *
 *                                                                           *
 ****************************************************************************/

///Called when a new collection is added
void AkonadiBackend::collectionsReceived( const Akonadi::Collection::List&  list)
{
   foreach (Akonadi::Collection coll, list) {
      update(coll);
      emit collectionChanged();
   }
}

///Update the contact list even without a new collection
ContactList AkonadiBackend::update_slot()
{
   return update(m_Collection);
}