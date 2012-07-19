/************************************************************************************
 *   Copyright (C) 2011 by Savoir-Faire Linux                                       *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/

//Parent
#include "AkonadiBackend.h"

//Qt
#include <QtCore/QTimer>
#include <QtCore/QObject>

//KDE
#include <KDebug>
#include <kdialog.h>
#include <akonadi/control.h>
#include <akonadi/collectionfilterproxymodel.h>
#include <akonadi/kmime/messagemodel.h>
#include <akonadi/recursiveitemfetchjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>
#include <akonadi/contact/contacteditor.h>
#include <akonadi/contact/contacteditordialog.h>
#include <akonadi/session.h>
#include <kabc/addressee.h>
#include <kabc/addresseelist.h>
#include <kabc/contactgroup.h>
#include <kabc/phonenumber.h>

//SFLPhone library
#include "../lib/Contact.h"
#include "../lib/AccountList.h"
#include "../lib/Account.h"

///Init static attributes
AkonadiBackend*  AkonadiBackend::m_pInstance = nullptr;

///Constructor
AkonadiBackend::AkonadiBackend(QObject* parent) : ContactBackend(parent)
{
   m_pSession = new Akonadi::Session( "SFLPhone::instance" );

   // fetching all collections containing emails recursively, starting at the root collection
   Akonadi::CollectionFetchJob *job = new Akonadi::CollectionFetchJob( Akonadi::Collection::root(), Akonadi::CollectionFetchJob::Recursive, this );
   job->fetchScope().setContentMimeTypes( QStringList() << "text/directory" );
   connect( job, SIGNAL( collectionsReceived( const Akonadi::Collection::List& ) ), this, SLOT( collectionsReceived( const Akonadi::Collection::List& ) ) );
} //AkonadiBackend

///Destructor
AkonadiBackend::~AkonadiBackend()
{
   CallModel<>::destroy();
   delete m_pSession;
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
///@param resolveDNS check if the DNS is used by an account, then assume contact with that phone number / extension is the same as the caller
Contact* AkonadiBackend::getContactByPhone(const QString& phoneNumber,bool resolveDNS,Account* a)
{
   QString number = phoneNumber;
   if (number.left(5) == "<sip:")
      number = number.remove(0,5);
   if (number.right(1) == ">")
      number = number.remove(number.size()-1,1);
   Contact* c = m_ContactByPhone[number];
   if (c) {
      return c;
   }
   if (!a)
      a = AccountList::getInstance()->getDefaultAccount();
   else if (number.indexOf("@") == -1 && a)
      return m_ContactByPhone[number+"@"+a->getAccountHostname()];
   
   if (resolveDNS &&  number.indexOf("@") != -1 && !getHostNameFromPhone(number).isEmpty() && m_ContactByPhone[getUserFromPhone(number)]) {
      foreach (Account* a, AccountList::getInstance()->getAccounts()) {
         if (a->getAccountHostname() == getHostNameFromPhone(number))
            return m_ContactByPhone[getUserFromPhone(number)];
      }
   }
   return nullptr;
} //getContactByPhone

///Find contact by UID
Contact* AkonadiBackend::getContactByUid(const QString& uid)
{
   return m_ContactByUid[uid];
}


/*****************************************************************************
 *                                                                           *
 *                                  Helper                                   *
 *                                                                           *
 ****************************************************************************/

KABC::PhoneNumber::Type nameToType(QString name)
{
   if      (name == "Home"   ) return KABC::PhoneNumber::Home ;
   else if (name == "Work"   ) return KABC::PhoneNumber::Work ;
   else if (name == "Msg"    ) return KABC::PhoneNumber::Msg  ;
   else if (name == "Pref"   ) return KABC::PhoneNumber::Pref ;
   else if (name == "Voice"  ) return KABC::PhoneNumber::Voice;
   else if (name == "Fax"    ) return KABC::PhoneNumber::Fax  ;
   else if (name == "Cell"   ) return KABC::PhoneNumber::Cell ;
   else if (name == "Video"  ) return KABC::PhoneNumber::Video;
   else if (name == "Bbs"    ) return KABC::PhoneNumber::Bbs  ;
   else if (name == "Modem"  ) return KABC::PhoneNumber::Modem;
   else if (name == "Car"    ) return KABC::PhoneNumber::Car  ;
   else if (name == "Isdn"   ) return KABC::PhoneNumber::Isdn ;
   else if (name == "Pcs"    ) return KABC::PhoneNumber::Pcs  ;
   else if (name == "Pager"  ) return KABC::PhoneNumber::Pager;
   return KABC::PhoneNumber::Home;
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Update the contact list when a new Akonadi collection is added
ContactList AkonadiBackend::update(Akonadi::Collection collection)
{
   Account* defaultAccount = AccountList::getInstance()->getDefaultAccount();
   m_Collection = collection;
   if ( !collection.isValid() ) {
      kDebug() << "The current collection is not valid";
      return ContactList();
   }

   Akonadi::RecursiveItemFetchJob *job = new Akonadi::RecursiveItemFetchJob( collection, QStringList() << KABC::Addressee::mimeType() << KABC::ContactGroup::mimeType());
   job->fetchScope().fetchFullPayload();
   if ( job->exec() ) {

      const Akonadi::Item::List items = job->items();

      foreach ( const Akonadi::Item &item, items ) {
         if ( item.hasPayload<KABC::ContactGroup>() ) {
            kDebug() << "Group:" << item.payload<KABC::ContactGroup>().name();
         }

         if ( item.hasPayload<KABC::Addressee>() ) {
            KABC::Addressee tmp = item.payload<KABC::Addressee>();
            Contact* aContact   = new Contact();

            KABC::PhoneNumber::List numbers = tmp.phoneNumbers();
            PhoneNumbers newNumbers;
            foreach (KABC::PhoneNumber number, numbers) {
               newNumbers << new Contact::PhoneNumber(number.number(),number.typeLabel());
               QString number2 = number.number();
               if (number2.left(5) == "<sip:")
                  number2 = number2.remove(0,5);
               if (number2.right(1) == ">")
                  number2 = number2.remove(number2.size()-1,1);
               m_ContactByPhone[number2] = aContact;
               if (number2.size() <= 6 && defaultAccount && !defaultAccount->getAccountHostname().isEmpty())
                  m_ContactByPhone[number2+"@"+defaultAccount->getAccountHostname()] = aContact;
            }
            m_ContactByUid[tmp.uid()] = aContact;

            aContact->setNickName       (tmp.nickName()       );
            aContact->setFormattedName  (tmp.formattedName()  );
            aContact->setFirstName      (tmp.givenName()      );
            aContact->setFamilyName     (tmp.familyName()     );
            aContact->setOrganization   (tmp.organization()   );
            aContact->setPreferredEmail (tmp.preferredEmail() );
            aContact->setDepartment     (tmp.department()     );
            aContact->setUid            (tmp.uid()            );
            aContact->setPhoneNumbers   (newNumbers           );

            if (!tmp.photo().data().isNull())
               aContact->setPhoto(new QPixmap(QPixmap::fromImage( tmp.photo().data()).scaled(QSize(48,48))));
            else
               aContact->setPhoto(0);
            
            m_AddrHash[tmp.uid()] = tmp;
            m_ItemHash[tmp.uid()] = item;
         }
      }
      m_pContacts = m_ContactByUid.values();
   }
   return m_ContactByUid.values();
} //update

///Edit backend value using an updated frontend contact
void AkonadiBackend::editContact(Contact* contact,QWidget* parent)
{
   Akonadi::Item item = m_ItemHash[contact->getUid()];
   if (!(item.hasPayload<KABC::Addressee>() && item.payload<KABC::Addressee>().uid() == contact->getUid())) {
      kDebug() << "Contact not found";
      return;
   }
   
   if ( item.isValid() ) {
      Akonadi::ContactEditor *editor = new Akonadi::ContactEditor( Akonadi::ContactEditor::EditMode, parent );
      editor->loadContact(item);
      KDialog* dlg = new KDialog(parent);
      dlg->setMainWidget(editor);
      dlg->exec();
      if ( !editor->saveContact() ) {
         kDebug() << "Unable to save new contact to storage";
         return;
      }
      delete editor;
      delete dlg;
   }
} //editContact

///Add a new contact
void AkonadiBackend::addNewContact(Contact* contact,QWidget* parent)
{
   KABC::Addressee newContact;
   newContact.setNickName       ( contact->getNickName()        );
   newContact.setFormattedName  ( contact->getFormattedName()   );
   newContact.setGivenName      ( contact->getFirstName()       );
   newContact.setFamilyName     ( contact->getSecondName()      );
   newContact.setOrganization   ( contact->getOrganization()    );
   newContact.setDepartment     ( contact->getDepartment()      );
   //newContact.setPreferredEmail ( contact->getPreferredEmail()  );//TODO

   foreach (Contact::PhoneNumber* nb, contact->getPhoneNumbers()) {
      KABC::PhoneNumber pn;
      pn.setType(nameToType(nb->getType()));

      pn.setNumber(nb->getNumber());
      newContact.insertPhoneNumber(pn);
   }

   //aContact->setPhoneNumbers   (newNumbers           );//TODO

   Akonadi::ContactEditor *editor = new Akonadi::ContactEditor( Akonadi::ContactEditor::CreateMode, parent );

   editor->setContactTemplate(newContact);

   KDialog* dlg = new KDialog(parent);
   dlg->setMainWidget(editor);
   dlg->exec();

   if ( !editor->saveContact() ) {
      kDebug() << "Unable to save new contact to storage";
      return;
   }
} //addNewContact

///Implement virtual pure method
void AkonadiBackend::editContact(Contact* contact)
{
   editContact(contact,0);
}

///Implement virtual pure method
void AkonadiBackend::addNewContact(Contact* contact)
{
   addNewContact(contact,0);
}

///Add a new phone number to an existing contact
void AkonadiBackend::addPhoneNumber(Contact* contact, QString number, QString type)
{
   Akonadi::Item item = m_ItemHash[contact->getUid()];
   if (!(item.hasPayload<KABC::Addressee>() && item.payload<KABC::Addressee>().uid() == contact->getUid())) {
      kDebug() << "Contact not found";
      return;
   }
   if ( item.isValid() ) {
      KABC::Addressee payload = item.payload<KABC::Addressee>();
      payload.insertPhoneNumber(KABC::PhoneNumber(number,nameToType(type)));
      item.setPayload<KABC::Addressee>(payload);
      Akonadi::ContactEditor *editor = new Akonadi::ContactEditor( Akonadi::ContactEditor::EditMode, (QWidget*)nullptr );
      editor->loadContact(item);

      KDialog* dlg = new KDialog(0);
      dlg->setMainWidget(editor);
      dlg->exec();
      if ( !editor->saveContact() ) {
         kDebug() << "Unable to save new contact to storage";
         return;
      }
      delete editor;
   }
   else {
      kDebug() << "Invalid item";
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
   return m_pContacts;//update(m_Collection);
}