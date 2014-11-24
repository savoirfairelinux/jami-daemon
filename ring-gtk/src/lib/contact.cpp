/****************************************************************************
 *   Copyright (C) 2009-2014 by Savoir-Faire Linux                          *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

//Parent
#include "contact.h"

//SFLPhone library
#include "sflphone_const.h"
#include "phonenumber.h"
#include "account.h"
#include "vcardutils.h"
#include "abstractitembackend.h"
#include "transitionalcontactbackend.h"
#include "numbercategorymodel.h"
#include "numbercategory.h"
#include "visitors/pixmapmanipulationvisitor.h"

class ContactPrivate {
public:
   ContactPrivate(Contact* contact, AbstractContactBackend* parent);
   ~ContactPrivate();
   QString                 m_FirstName      ;
   QString                 m_SecondName     ;
   QString                 m_NickName       ;
   QVariant                m_pPhoto         ;
   QString                 m_FormattedName  ;
   QString                 m_PreferredEmail ;
   QString                 m_Organization   ;
   QByteArray              m_Uid            ;
   QString                 m_Group          ;
   QString                 m_Department     ;
   bool                    m_DisplayPhoto   ;
   Contact::PhoneNumbers   m_Numbers        ;
   bool                    m_Active         ;
   AbstractContactBackend* m_pBackend       ;
   bool                    m_isPlaceHolder  ;

   //Cache
   QString m_CachedFilterString;

   QString filterString();

   //Helper code to help handle multiple parents
   QList<Contact*> m_lParents;

   //As a single D-Pointer can have multiple parent (when merged), all emit need
   //to use a proxy to make sure everybody is notified
   void presenceChanged( PhoneNumber* );
   void statusChanged  ( bool         );
   void changed        (              );
   void phoneNumberCountChanged(int,int);
   void phoneNumberCountAboutToChange(int,int);
};

QString ContactPrivate::filterString()
{
   if (m_CachedFilterString.size())
      return m_CachedFilterString;

   //Also filter by phone numbers, accents are negligible
   foreach(const PhoneNumber* n , m_Numbers) {
      m_CachedFilterString += n->uri();
   }

   //Strip non essential characters like accents from the filter string
   foreach(const QChar& char2,QString(m_FormattedName+'\n'+m_Organization+'\n'+m_Group+'\n'+
      m_Department+'\n'+m_PreferredEmail).toLower().normalized(QString::NormalizationForm_KD) ) {
      if (!char2.combiningClass())
         m_CachedFilterString += char2;
   }

   return m_CachedFilterString;
}

void ContactPrivate::changed()
{
   m_CachedFilterString.clear();
   foreach (Contact* c,m_lParents) {
      emit c->changed();
   }
}

void ContactPrivate::presenceChanged( PhoneNumber* n )
{
   foreach (Contact* c,m_lParents) {
      emit c->presenceChanged(n);
   }
}

void ContactPrivate::statusChanged  ( bool s )
{
   foreach (Contact* c,m_lParents) {
      emit c->statusChanged(s);
   }
}

void ContactPrivate::phoneNumberCountChanged(int n,int o)
{
   foreach (Contact* c,m_lParents) {
      emit c->phoneNumberCountChanged(n,o);
   }
}

void ContactPrivate::phoneNumberCountAboutToChange(int n,int o)
{
   foreach (Contact* c,m_lParents) {
      emit c->phoneNumberCountAboutToChange(n,o);
   }
}

ContactPrivate::ContactPrivate(Contact* contact, AbstractContactBackend* parent):m_Numbers(contact),
   m_DisplayPhoto(nullptr),m_Active(true),
   m_pBackend(parent?parent:TransitionalContactBackend::instance())
{}

ContactPrivate::~ContactPrivate()
{

}

Contact::PhoneNumbers::PhoneNumbers(Contact* parent) : QVector<PhoneNumber*>(),CategorizedCompositeNode(CategorizedCompositeNode::Type::NUMBER),
    m_pParent2(parent)
{
}

Contact::PhoneNumbers::PhoneNumbers(Contact* parent, const QVector<PhoneNumber*>& list)
: QVector<PhoneNumber*>(list),CategorizedCompositeNode(CategorizedCompositeNode::Type::NUMBER),m_pParent2(parent)
{
}

Contact* Contact::PhoneNumbers::contact() const
{
   return m_pParent2;
}

///Constructor
Contact::Contact(AbstractContactBackend* parent):QObject(parent?parent:TransitionalContactBackend::instance()),
   d(new ContactPrivate(this,parent))
{
   d->m_isPlaceHolder = false;
   d->m_lParents << this;
}

///Constructor from VCard
//Contact::Contact(AbstractContactBackend *parent, const QByteArray& vcard):QObject(parent?parent:TransitionalContactBackend::instance()),
//   d(new ContactPrivate(this,parent))
//{
//   QHash<VCProperty, void*> dictionary;
//   dictionary[VCProperty::VC_FORMATTED_NAME] = setFormattedName();
//   dictionary[VCProperty::VC_NAME]           = setFamilyName();

//   Q_UNUSED(vcard)
//   d->m_isPlaceHolder = false;
//   d->m_lParents << this;
//   setFormattedName("ALRIGHT");
//}

///Destructor
Contact::~Contact()
{
   //Unregister itself from the D-Pointer list
   d->m_lParents.removeAll(this);

   if (!d->m_lParents.size()) {
      delete d;
   }
}

///Get the phone number list
const Contact::PhoneNumbers& Contact::phoneNumbers() const
{
   return d->m_Numbers;
}

///Get the nickname
const QString& Contact::nickName() const
{
   return d->m_NickName;
}

///Get the firstname
const QString& Contact::firstName() const
{
   return d->m_FirstName;
}

///Get the second/family name
const QString& Contact::secondName() const
{
   return d->m_SecondName;
}

///Get the photo
const QVariant Contact::photo() const
{
   return d->m_pPhoto;
}

///Get the formatted name
const QString& Contact::formattedName() const
{
   return d->m_FormattedName;
}

///Get the organisation
const QString& Contact::organization()  const
{
   return d->m_Organization;
}

///Get the preferred email
const QString& Contact::preferredEmail()  const
{
   return d->m_PreferredEmail;
}

///Get the unique identifier (used for drag and drop)
const QByteArray& Contact::uid() const
{
   return d->m_Uid;
}

///Get the group
const QString& Contact::group() const
{
   return d->m_Group;
}

const QString& Contact::department() const
{
   return d->m_Department;
}

///Set the phone number (type and number)
void Contact::setPhoneNumbers(PhoneNumbers numbers)
{
   const int oldCount(d->m_Numbers.size()),newCount(numbers.size());
   foreach(PhoneNumber* n, d->m_Numbers)
      disconnect(n,SIGNAL(presentChanged(bool)),this,SLOT(slotPresenceChanged()));
   d->m_Numbers = numbers;
   if (newCount < oldCount) //Rows need to be removed from models first
      d->phoneNumberCountAboutToChange(newCount,oldCount);
   foreach(PhoneNumber* n, d->m_Numbers)
      connect(n,SIGNAL(presentChanged(bool)),this,SLOT(slotPresenceChanged()));
   if (newCount > oldCount) //Need to be updated after the data to prevent invalid memory access
      d->phoneNumberCountChanged(newCount,oldCount);
   d->changed();
}

///Set the nickname
void Contact::setNickName(const QString& name)
{
   d->m_NickName = name;
   d->changed();
}

///Set the first name
void Contact::setFirstName(const QString& name)
{
   d->m_FirstName = name;
   setObjectName(formattedName());
   d->changed();
}

///Set the family name
void Contact::setFamilyName(const QString& name)
{
   d->m_SecondName = name;
   setObjectName(formattedName());
   d->changed();
}

///Set the Photo/Avatar
void Contact::setPhoto(const QVariant& photo)
{
   d->m_pPhoto = photo;
   d->changed();
}

///Set the formatted name (display name)
void Contact::setFormattedName(const QString& name)
{
   d->m_FormattedName = name;
   d->changed();
}

///Set the organisation / business
void Contact::setOrganization(const QString& name)
{
   d->m_Organization = name;
   d->changed();
}

///Set the default email
void Contact::setPreferredEmail(const QString& name)
{
   d->m_PreferredEmail = name;
   d->changed();
}

///Set UID
void Contact::setUid(const QByteArray& id)
{
   d->m_Uid = id;
   d->changed();
}

///Set Group
void Contact::setGroup(const QString& name)
{
   d->m_Group = name;
   d->changed();
}

///Set department
void Contact::setDepartment(const QString& name)
{
   d->m_Department = name;
   d->changed();
}

///If the contact have been deleted or not yet fully created
void Contact::setActive( bool active)
{
   d->m_Active = active;
   d->statusChanged(d->m_Active);
   d->changed();
}

///Return if one of the PhoneNumber is present
bool Contact::isPresent() const
{
   foreach(const PhoneNumber* n,d->m_Numbers) {
      if (n->isPresent())
         return true;
   }
   return false;
}

///Return if one of the PhoneNumber is tracked
bool Contact::isTracked() const
{
   foreach(const PhoneNumber* n,d->m_Numbers) {
      if (n->isTracked())
         return true;
   }
   return false;
}

///Have this contact been deleted or doesn't exist yet
bool Contact::isActive() const
{
   return d->m_Active;
}

///Return if one of the PhoneNumber support presence
bool Contact::supportPresence() const
{
   foreach(const PhoneNumber* n,d->m_Numbers) {
      if (n->supportPresence())
         return true;
   }
   return false;
}


QObject* Contact::PhoneNumbers::getSelf() const {
   return m_pParent2;
}

time_t Contact::PhoneNumbers::lastUsedTimeStamp() const
{
   time_t t = 0;
   for (int i=0;i<size();i++) {
      if (at(i)->lastUsed() > t)
         t = at(i)->lastUsed();
   }
   return t;
}

///Recomputing the filter string is heavy, cache it
QString Contact::filterString() const
{
   return d->filterString();
}

///Callback when one of the phone number presence change
void Contact::slotPresenceChanged()
{
   d->changed();
}

///Save the contact
bool Contact::save() const
{
   return d->m_pBackend->save(this);
}

///Show an implementation dependant dialog to edit the contact
bool Contact::edit()
{
   return d->m_pBackend->edit(this);
}

///Remove the contact from the backend
bool Contact::remove()
{
   return d->m_pBackend->remove(this);
}

///Add a new phone number to the backend
///@note The backend is expected to notify the Contact (asynchronously) when done
bool Contact::addPhoneNumber(PhoneNumber* n)
{
   return d->m_pBackend->addPhoneNumber(this,n);
}

///Create a placeholder contact, it will eventually be replaced when the real one is loaded
ContactPlaceHolder::ContactPlaceHolder(const QByteArray& uid)
{
   setUid(uid);
   d->m_isPlaceHolder = true;
}


bool ContactPlaceHolder::merge(Contact* contact)
{
   if ((!contact) || ((*contact) == this))
      return false;

   ContactPrivate* currentD = d;
   replaceDPointer(contact);
   currentD->m_lParents.removeAll(this);
   if (!currentD->m_lParents.size())
      delete currentD;
   return true;
}

void Contact::replaceDPointer(Contact* c)
{
   this->d = c->d;
   d->m_lParents << this;
   emit changed();
   emit rebased(c);
}

const QByteArray Contact::toVCard(QList<Account*> accounts) const
{
   //serializing here
   VCardUtils* maker = new VCardUtils();
   maker->startVCard("2.1");
   maker->addProperty(VCardUtils::Property::UID, uid());
   maker->addProperty(VCardUtils::Property::NAME, QString(secondName()
                                                   + VCardUtils::Delimiter::SEPARATOR_TOKEN
                                                   + firstName()));
   maker->addProperty(VCardUtils::Property::FORMATTED_NAME, formattedName());
   maker->addProperty(VCardUtils::Property::MAILER, preferredEmail());
   maker->addProperty(VCardUtils::Property::ORGANIZATION, organization());

   for (PhoneNumber* phone : phoneNumbers()) {
      maker->addPhoneNumber(phone->category()->name(), phone->uri());
   }

   for(Account* acc : accounts) {
      maker->addProperty(VCardUtils::Property::X_RINGACCOUNT, acc->id());
   }

   maker->addPhoto(PixmapManipulationVisitor::instance()->toByteArray(photo()).simplified());
   return maker->endVCard();
}

bool Contact::operator==(Contact* other)
{
   return other && this->d == other->d;
}

bool Contact::operator==(const Contact* other) const
{
   return other && this->d == other->d;
}

bool Contact::operator==(Contact& other)
{
   return &other && this->d == other.d;
}

bool Contact::operator==(const Contact& other) const
{
   return &other && this->d == other.d;
}
