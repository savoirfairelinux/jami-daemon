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

//SFLPhone
//#include "SFLPhone.h"
//#include "SFLPhoneView.h"

///Init static attributes
AkonadiBackend*  AkonadiBackend::m_pInstance = 0;
CallModel<>*     AkonadiBackend::m_pModel    = 0;

///Constructor
AkonadiBackend::AkonadiBackend(QObject* parent) : ContactBackend(parent)
{
   //QTimer::singleShot( 0, this, SLOT( delayedInit() ) );
   m_pSession = new Akonadi::Session( "SFLPhone::instance" );

   if ( not m_pModel ) {
      m_pModel = new CallModel<>();
      m_pModel->initCall();
      //m_pModel->initHistory();
   }

   // fetching all collections containing emails recursively, starting at the root collection
   Akonadi::CollectionFetchJob *job = new Akonadi::CollectionFetchJob( Akonadi::Collection::root(), Akonadi::CollectionFetchJob::Recursive, this );
   job->fetchScope().setContentMimeTypes( QStringList() << "text/directory" );
   connect( job, SIGNAL( collectionsReceived( const Akonadi::Collection::List& ) ), this, SLOT( collectionsReceived( const Akonadi::Collection::List& ) ) );
} //AkonadiBackend

///Destructor
AkonadiBackend::~AkonadiBackend()
{
   delete m_pModel;
   CallModel<>::destroy();
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
Contact* AkonadiBackend::getContactByPhone(const QString& phoneNumber,bool resolveDNS)
{
   if (!resolveDNS || phoneNumber.indexOf("@") == -1)
      return m_ContactByPhone[phoneNumber];
   else if (!getHostNameFromPhone(phoneNumber).isEmpty() && m_ContactByPhone[getUserFromPhone(phoneNumber)]) {
      foreach (Account* a, m_pModel->getAccountList()->getAccounts()) {
         if (a->getAccountDetail(ACCOUNT_HOSTNAME) == getHostNameFromPhone(phoneNumber))
            return m_ContactByPhone[getUserFromPhone(phoneNumber)];
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
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Update the contact list when a new Akonadi collection is added
ContactList AkonadiBackend::update(Akonadi::Collection collection)
{
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
               m_ContactByPhone[number.number()] = aContact;
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
         }
      }
      m_pContacts = m_ContactByUid.values();
   }
   return m_ContactByUid.values();
} //update

///Edit backend value using an updated frontend contact
void AkonadiBackend::editContact(Contact* contact,QWidget* parent)
{
   KABC::Addressee ct = m_AddrHash[contact->getUid()];
   if (ct.uid() != contact->getUid()) {
      kDebug() << "Contact not found";
      return;
   }
   
   Akonadi::ContactEditorDialog *editor = new Akonadi::ContactEditorDialog( Akonadi::ContactEditorDialog::EditMode, parent );
   Akonadi::Item item(rand());
   item.setPayload<KABC::Addressee>(ct);
   if ( item.isValid() ) {
      editor->setContact(item);
      editor->exec();
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

/*****************************************************************************
 *                                                                           *
 *                                  Helpers                                  *
 *                                                                           *
 ****************************************************************************/

///Return the extension/user of an URI (<sip:12345@exemple.com>)
QString AkonadiBackend::getUserFromPhone(QString phoneNumber)
{
   if (phoneNumber.indexOf("@") != -1) {
      QString user = phoneNumber.split("@")[0];
      if (user.indexOf(":") != -1) {
         return user.split(":")[1];
      }
      else {
         return user;
      }
   }
   return phoneNumber;
} //getUserFromPhone

///Return the domaine of an URI (<sip:12345@exemple.com>)
QString AkonadiBackend::getHostNameFromPhone(QString phoneNumber)
{
   if (phoneNumber.indexOf("@") != -1) {
      return phoneNumber.split("@")[1].left(phoneNumber.split("@")[1].size()-1);
   }
   return "";
}