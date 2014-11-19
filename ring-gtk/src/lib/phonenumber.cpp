/****************************************************************************
 *   Copyright (C) 2013-2014 by Savoir-Faire Linux                          *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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
#include "phonenumber.h"
#include "phonedirectorymodel.h"
#include "contact.h"
#include "account.h"
#include "call.h"
#include "dbus/presencemanager.h"
#include "numbercategorymodel.h"
#include "numbercategory.h"

QHash<int,Call*> PhoneNumber::m_shMostUsed = QHash<int,Call*>();

const PhoneNumber* PhoneNumber::m_spBlank = nullptr;

class PrivatePhoneNumber {
public:
   PrivatePhoneNumber(const URI& number, NumberCategory* cat, PhoneNumber::Type st);
   NumberCategory*    m_pCategory        ;
   bool               m_Present          ;
   QString            m_PresentMessage   ;
   bool               m_Tracked          ;
   Contact*           m_pContact         ;
   Account*           m_pAccount         ;
   time_t             m_LastUsed         ;
   QList<Call*>       m_lCalls           ;
   int                m_PopularityIndex  ;
   QString            m_MostCommonName   ;
   QHash<QString,int> m_hNames           ;
   bool               m_hasType          ;
   uint               m_LastWeekCount    ;
   uint               m_LastTrimCount    ;
   bool               m_HaveCalled       ;
   int                m_Index            ;
   bool               m_IsBookmark       ;
   int                m_TotalSeconds     ;
   QString            m_Uid              ;
   QString            m_PrimaryName_cache;
   URI                m_Uri              ;
   PhoneNumber::Type  m_Type             ;
   QList<URI>         m_lOtherURIs       ;

   //Parents
   QList<PhoneNumber*> m_lParents;

   //Emit proxies
   void callAdded(Call* call);
   void changed  (          );
   void presentChanged(bool);
   void presenceMessageChanged(const QString&);
   void trackedChanged(bool);
   void primaryNameChanged(const QString& name);
   void rebased(PhoneNumber* other);
};

void PrivatePhoneNumber::callAdded(Call* call)
{
   foreach (PhoneNumber* n, m_lParents)
      emit n->callAdded(call);
}

void PrivatePhoneNumber::changed()
{
   foreach (PhoneNumber* n, m_lParents)
      emit n->changed();
}

void PrivatePhoneNumber::presentChanged(bool s)
{
   foreach (PhoneNumber* n, m_lParents)
      emit n->presentChanged(s);
}

void PrivatePhoneNumber::presenceMessageChanged(const QString& status)
{
   foreach (PhoneNumber* n, m_lParents)
      emit n->presenceMessageChanged(status);
}

void PrivatePhoneNumber::trackedChanged(bool t)
{
   foreach (PhoneNumber* n, m_lParents)
      emit n->trackedChanged(t);
}

void PrivatePhoneNumber::primaryNameChanged(const QString& name)
{
   foreach (PhoneNumber* n, m_lParents)
      emit n->primaryNameChanged(name);
}

void PrivatePhoneNumber::rebased(PhoneNumber* other)
{
   foreach (PhoneNumber* n, m_lParents)
      emit n->rebased(other);
}


const PhoneNumber* PhoneNumber::BLANK()
{
   if (!m_spBlank) {
      m_spBlank = new PhoneNumber(QString(),NumberCategoryModel::other());
      const_cast<PhoneNumber*>(m_spBlank)->d->m_Type = PhoneNumber::Type::BLANK;
   }
   return m_spBlank;
}

PrivatePhoneNumber::PrivatePhoneNumber(const URI& uri, NumberCategory* cat, PhoneNumber::Type st) :
   m_Uri(uri),m_pCategory(cat),m_Tracked(false),m_Present(false),m_LastUsed(0),
   m_Type(st),m_PopularityIndex(-1),m_pContact(nullptr),m_pAccount(nullptr),
   m_LastWeekCount(0),m_LastTrimCount(0),m_HaveCalled(false),m_IsBookmark(false),m_TotalSeconds(0),
   m_Index(-1)
{}

///Constructor
PhoneNumber::PhoneNumber(const URI& number, NumberCategory* cat, Type st) : QObject(PhoneDirectoryModel::instance()),
d(new PrivatePhoneNumber(number,cat,st))
{
   setObjectName(d->m_Uri);
   d->m_hasType = cat != NumberCategoryModel::other();
   if (d->m_hasType) {
      NumberCategoryModel::instance()->registerNumber(this);
   }
   d->m_lParents << this;
}

PhoneNumber::~PhoneNumber()
{
   d->m_lParents.removeAll(this);
   if (!d->m_lParents.size())
      delete d;
}

///Return if this number presence is being tracked
bool PhoneNumber::isTracked() const
{
   //If the number doesn't support it, ignore the flag
   return supportPresence() && d->m_Tracked;
}

///Is this number present
bool PhoneNumber::isPresent() const
{
   return d->m_Tracked && d->m_Present;
}

///This number presence status string
QString PhoneNumber::presenceMessage() const
{
   return d->m_PresentMessage;
}

///Return the number
URI PhoneNumber::uri() const {
   return d->m_Uri ;
}

///This phone number has a type
bool PhoneNumber::hasType() const
{
   return d->m_hasType;
}

///Protected getter to get the number index
int PhoneNumber::index() const
{
   return d->m_Index;
}

///Return the phone number type
NumberCategory* PhoneNumber::category() const {
   return d->m_pCategory ;
}

///Return this number associated account, if any
Account* PhoneNumber::account() const
{
   return d->m_pAccount;
}

///Return this number associated contact, if any
Contact* PhoneNumber::contact() const
{
   return d->m_pContact;
}

///Return when this number was last used
time_t PhoneNumber::lastUsed() const
{
   return d->m_LastUsed;
}

///Set this number default account
void PhoneNumber::setAccount(Account* account)
{
   d->m_pAccount = account;
   if (d->m_pAccount)
      connect (d->m_pAccount,SIGNAL(destroyed(QObject*)),this,SLOT(accountDestroyed(QObject*)));
   d->changed();
}

///Set this number contact
void PhoneNumber::setContact(Contact* contact)
{
   d->m_pContact = contact;
   if (contact && d->m_Type != PhoneNumber::Type::TEMPORARY) {
      PhoneDirectoryModel::instance()->indexNumber(this,d->m_hNames.keys()+QStringList(contact->formattedName()));
      d->m_PrimaryName_cache = contact->formattedName();
      d->primaryNameChanged(d->m_PrimaryName_cache);
      connect(contact,SIGNAL(rebased(Contact*)),this,SLOT(contactRebased(Contact*)));
   }
   d->changed();
}

///Protected setter to set if there is a type
void PhoneNumber::setHasType(bool value)
{
   d->m_hasType = value;
}

///Protected setter to set the PhoneDirectoryModel index
void PhoneNumber::setIndex(int value)
{
   d->m_Index = value;
}

///Protected setter to change the popularity index
void PhoneNumber::setPopularityIndex(int value)
{
   d->m_PopularityIndex = value;
}

void PhoneNumber::setCategory(NumberCategory* cat)
{
   if (cat == d->m_pCategory) return;
   if (d->m_hasType)
      NumberCategoryModel::instance()->unregisterNumber(this);
   d->m_hasType = cat != NumberCategoryModel::other();
   d->m_pCategory = cat;
   if (d->m_hasType)
      NumberCategoryModel::instance()->registerNumber(this);
   d->changed();
}

void PhoneNumber::setBookmarked(bool bookmarked )
{
   d->m_IsBookmark = bookmarked;
}

///Force an Uid on this number (instead of hash)
void PhoneNumber::setUid(const QString& uri)
{
   d->m_Uid = uri;
}

///Attempt to change the number type
bool PhoneNumber::setType(PhoneNumber::Type t)
{
   if (d->m_Type == PhoneNumber::Type::BLANK)
      return false;
   if (account() && t == PhoneNumber::Type::ACCOUNT) {
      if (account()->supportPresenceSubscribe()) {
         d->m_Tracked = true; //The daemon will init the tracker itself
         d->trackedChanged(true);
      }
      d->m_Type = t;
      return true;
   }
   return false;
}

///Set if this number is tracking presence information
void PhoneNumber::setTracked(bool track)
{
   if (track != d->m_Tracked) { //Subscribe only once
      //You can't subscribe without account
      if (track && !d->m_pAccount) return;
      d->m_Tracked = track;
      DBus::PresenceManager::instance().subscribeBuddy(d->m_pAccount->id(),uri().fullUri(),track);
      d->changed();
      d->trackedChanged(track);
   }
}

///Allow phonedirectorymodel to change presence status
void PhoneNumber::setPresent(bool present)
{
   if (d->m_Present != present) {
      d->m_Present = present;
      d->presentChanged(present);
   }
}

void PhoneNumber::setPresenceMessage(const QString& message)
{
   if (d->m_PresentMessage != message) {
      d->m_PresentMessage = message;
      d->presenceMessageChanged(message);
   }
}

///Return the current type of the number
PhoneNumber::Type PhoneNumber::type() const
{
   return d->m_Type;
}

///Return the number of calls from this number
int PhoneNumber::callCount() const
{
   return d->m_lCalls.size();
}

uint PhoneNumber::weekCount() const
{
   return d->m_LastWeekCount;
}

uint PhoneNumber::trimCount() const
{
   return d->m_LastTrimCount;
}

bool PhoneNumber::haveCalled() const
{
   return d->m_HaveCalled;
}

///Best bet for this person real name
QString PhoneNumber::primaryName() const
{
   //Compute the primary name
   if (d->m_PrimaryName_cache.isEmpty()) {
      QString ret;
      if (d->m_hNames.size() == 1)
         ret =  d->m_hNames.constBegin().key();
      else {
         QString toReturn = tr("Unknown");
         int max = 0;
         for (QHash<QString,int>::const_iterator i = d->m_hNames.begin(); i != d->m_hNames.end(); ++i) {
            if (i.value() > max) {
               max      = i.value();
               toReturn = i.key  ();
            }
         }
         ret = toReturn;
      }
      const_cast<PhoneNumber*>(this)->d->m_PrimaryName_cache = ret;
      const_cast<PhoneNumber*>(this)->d->primaryNameChanged(d->m_PrimaryName_cache);
   }
   //Fallback: Use the URI
   if (d->m_PrimaryName_cache.isEmpty()) {
      return uri();
   }

   //Return the cached primaryname
   return d->m_PrimaryName_cache;
}

///Is this number bookmarked
bool PhoneNumber::isBookmarked() const
{
   return d->m_IsBookmark;
}

///If this number could (theoretically) support presence status
bool PhoneNumber::supportPresence() const
{
   //Without an account, presence is impossible
   if (!d->m_pAccount)
      return false;
   //The account also have to support it
   if (!d->m_pAccount->supportPresenceSubscribe())
       return false;

   //In the end, it all come down to this, is the number tracked
   return true;
}

///Proxy accessor to the category icon
QVariant PhoneNumber::icon() const
{
   return category()->icon(isTracked(),isPresent());
}

///The number of seconds spent with the URI (from history)
int PhoneNumber::totalSpentTime() const
{
   return d->m_TotalSeconds;
}

///Return this number unique identifier (hash)
QString PhoneNumber::uid() const
{
   return d->m_Uid.isEmpty()?toHash():d->m_Uid;
}

///Return all calls from this number
QList<Call*> PhoneNumber::calls() const
{
   return d->m_lCalls;
}

///Return the phonenumber position in the popularity index
int PhoneNumber::popularityIndex() const
{
   return d->m_PopularityIndex;
}

QHash<QString,int> PhoneNumber::alternativeNames() const
{
   return d->m_hNames;
}

///Add a call to the call list, notify listener
void PhoneNumber::addCall(Call* call)
{
   if (!call) return;
   d->m_Type = PhoneNumber::Type::USED;
   d->m_lCalls << call;
   d->m_TotalSeconds += call->stopTimeStamp() - call->startTimeStamp();
   time_t now;
   ::time ( &now );
   if (now - 3600*24*7 < call->stopTimeStamp())
      d->m_LastWeekCount++;
   if (now - 3600*24*7*15 < call->stopTimeStamp())
      d->m_LastTrimCount++;

   if (call->historyState() == Call::LegacyHistoryState::OUTGOING || call->direction() == Call::Direction::OUTGOING)
      d->m_HaveCalled = true;

   d->callAdded(call);
   if (call->startTimeStamp() > d->m_LastUsed)
      d->m_LastUsed = call->startTimeStamp();
   d->changed();
}

///Generate an unique representation of this number
QString PhoneNumber::toHash() const
{
   return QString("%1///%2///%3").arg(uri()).arg(account()?account()->id():QString()).arg(contact()?contact()->uid():QString());
}



///Increment name counter and update indexes
void PhoneNumber::incrementAlternativeName(const QString& name)
{
   const bool needReIndexing = !d->m_hNames[name];
   d->m_hNames[name]++;
   if (needReIndexing && d->m_Type != PhoneNumber::Type::TEMPORARY) {
      PhoneDirectoryModel::instance()->indexNumber(this,d->m_hNames.keys()+(d->m_pContact?(QStringList(d->m_pContact->formattedName())):QStringList()));
      //Invalid m_PrimaryName_cache
      if (!d->m_pContact)
         d->m_PrimaryName_cache.clear();
   }
}

void PhoneNumber::accountDestroyed(QObject* o)
{
   if (o == d->m_pAccount)
      d->m_pAccount = nullptr;
}

/**
 * When the PhoneNumber contact is merged with another one, the phone number
 * data might be replaced, like the preferred name.
 */
void PhoneNumber::contactRebased(Contact* other)
{
   d->m_PrimaryName_cache = other->formattedName();
   d->primaryNameChanged(d->m_PrimaryName_cache);
   d->changed();

   //It is a "partial" rebase, so the PhoneNumber data stay the same
   d->rebased(this);
}

/**
 * Merge two phone number to share the same data. This avoid having to change
 * pointers all over the place. The PhoneNumber objects remain intact, the
 * PhoneDirectoryModel will replace the old references, but existing ones will
 * keep working.
 */
bool PhoneNumber::merge(PhoneNumber* other)
{

   if ((!other) || other == this || other->d == d)
      return false;

   //This is invalid, those are different numbers
   if (account() && other->account() && account() != other->account())
      return false;

   //TODO Check if the merge is valid

   //TODO Merge the alternative names

   //TODO Handle presence

   PrivatePhoneNumber* currentD = d;

   //Replace the D-Pointer
   this->d = other->d;
   d->m_lParents << this;

   //In case the URI is different, take the longest and most precise
   //TODO keep a log of all URI used
   if (currentD->m_Uri.size() > other->d->m_Uri.size()) {
      other->d->m_lOtherURIs << other->d->m_Uri;
      other->d->m_Uri = currentD->m_Uri;
   }
   else
      other->d->m_lOtherURIs << currentD->m_Uri;

   emit changed();
   emit rebased(other);

   currentD->m_lParents.removeAll(this);
   if (!currentD->m_lParents.size())
      delete currentD;
   return true;
}

bool PhoneNumber::operator==(PhoneNumber* other)
{
   return other && this->d == other->d;
}

bool PhoneNumber::operator==(const PhoneNumber* other) const
{
   return other && this->d == other->d;
}

bool PhoneNumber::operator==(PhoneNumber& other)
{
   return &other && this->d == other.d;
}

bool PhoneNumber::operator==(const PhoneNumber& other) const
{
   return &other && this->d == other.d;
}

/************************************************************************************
 *                                                                                  *
 *                             Temporary phone number                               *
 *                                                                                  *
 ***********************************************************************************/

void TemporaryPhoneNumber::setUri(const QString& uri)
{
   d->m_Uri = uri;
   d->changed();
}

///Constructor
TemporaryPhoneNumber::TemporaryPhoneNumber(const PhoneNumber* number) :
   PhoneNumber(QString(),NumberCategoryModel::other(),PhoneNumber::Type::TEMPORARY)
{
   if (number) {
      setContact(number->contact());
      setAccount(number->account());
   }
}
