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
#include "profilemodel.h"

//SFLPhone library
#include "accountlistmodel.h"
#include "contactmodel.h"
#include "callmodel.h"
#include "abstractitembackend.h"
#include "visitors/profilepersistervisitor.h"
#include "visitors/pixmapmanipulationvisitor.h"
#include "vcardutils.h"

//Qt
#include <QtCore/QTimer>
#include <QtCore/QObject>
#include <QMimeData>
#include <QtCore/QPointer>

//Qt
class QObject;

//SFLPhone
class Contact;
class Account;
struct Node;

struct VCardMapper {
   VCardMapper() {
      m_hHash[VCardUtils::Property::UID] = &VCardMapper::setUid;
      m_hHash[VCardUtils::Property::NAME] = &VCardMapper::setNames;
      m_hHash[VCardUtils::Property::FORMATTED_NAME] = &VCardMapper::setFormattedName;
      m_hHash[VCardUtils::Property::ORGANIZATION] = &VCardMapper::setOrganization;
      m_hHash[VCardUtils::Property::PHOTO] = &VCardMapper::setPhoto;
   }

   void setFormattedName(Contact* c, const QByteArray& fn) {
      c->setFormattedName(QString::fromUtf8(fn));
   }

   void setNames(Contact* c, const QByteArray& fn) {
      QList<QByteArray> splitted = fn.split(';');
      c->setFamilyName(splitted[0].trimmed());
      c->setFirstName(splitted[1].trimmed());
   }

   void setUid(Contact* c, const QByteArray& fn) {
      c->setUid(fn);
   }

   void setEmail(Contact* c, const QByteArray& fn) {
      c->setPreferredEmail(fn);
   }

   void setOrganization(Contact* c, const QByteArray& fn) {
      c->setOrganization(QString::fromUtf8(fn));
   }

   void setPhoto(Contact* c, const QByteArray& fn) {
      qDebug() << fn;
      QVariant photo = PixmapManipulationVisitor::instance()->profilePhoto(fn);
      c->setPhoto(photo);
   }

   QHash<QByteArray, mapToProperty> m_hHash;
   bool metacall(Contact* c, const QByteArray& key, const QByteArray& value) {
      if (!m_hHash[key])
         return false;
      (this->*(m_hHash[key]))(c,value);
      return true;
   }
};
static VCardMapper* vc_mapper = new VCardMapper;

///ProfileContentBackend: Implement a backend for Profiles
class ProfileContentBackend : public AbstractContactBackend {
   Q_OBJECT
public:
   explicit ProfileContentBackend(QObject* parent);

   //Re-implementation
   virtual QString  name(                                                                     ) const override;
   virtual QVariant icon(                                                                     ) const override;
   virtual bool     isEnabled(                                                                ) const override;
   virtual bool     enable (bool enable                                                       ) override;
   virtual QByteArray  id  (                                                                  ) const override;
   virtual bool     edit   ( Contact* contact                                                 ) override;
   virtual bool     addNew ( Contact* contact                                                 ) override;
   virtual bool     remove ( Contact* c                                                       ) override;
   virtual bool     append (const Contact* item                                               ) override;
   virtual          ~ProfileContentBackend (                                                  );
   virtual bool     load(                                                                     ) override;
   virtual bool     reload(                                                                   ) override;
   virtual bool     save(const Contact* contact                                               ) override;
   SupportedFeatures supportedFeatures(                                                       ) const;
   virtual QList<Contact*> items(                                                             ) const override;
   virtual bool addPhoneNumber( Contact* contact , PhoneNumber* number                        ) override;

   //Attributes
   QList<Node*> m_lProfiles;
   QHash<QString,Node*> m_hProfileByAccountId;
   bool m_needSaving;

   QList<Contact*> m_bSaveBuffer;
   bool saveAll();

   QList<Account*> getAccountsForProfile(const QString& id);

   //Helper
   Node* getProfileById(const QByteArray& id);

public Q_SLOTS:
   void contactChanged();
   void save();
};


struct Node {
   Node(): account(nullptr),contact(nullptr),type(Node::Type::PROFILE), parent(nullptr),m_Index(0) {}

   enum class Type : bool {
      PROFILE,
      ACCOUNT,
   };

   Node*           parent;
   QVector<Node*>  children;
   Type            type;
   Account*        account;
   Contact*        contact;
   int             m_Index;
};


ProfileModel* ProfileModel::m_spInstance = nullptr;

ProfileContentBackend::ProfileContentBackend(QObject* parent) : AbstractContactBackend(this, parent)
{
}

QString ProfileContentBackend::name () const
{
   return tr("Profile backend");
}

QVariant ProfileContentBackend::icon() const
{
   return QVariant();
}

bool ProfileContentBackend::isEnabled() const
{
   return true;
}

bool ProfileContentBackend::enable (bool enable)
{
   Q_UNUSED(enable);
   static bool isLoaded = false;
   if (!isLoaded) {
     load();
     isLoaded = true;
   }
   return true;
}

QByteArray  ProfileContentBackend::id() const
{
   return "Profile_backend";
}

bool ProfileContentBackend::edit( Contact* contact )
{
   qDebug() << "Attempt to edit a profile contact" << contact->uid();
   return false;
}

bool ProfileContentBackend::addNew( Contact* contact )
{
   qDebug() << "Creating new profile" << contact->uid();
   save(contact);
   load();
   return true;
}

bool ProfileContentBackend::remove( Contact* c )
{
   Q_UNUSED(c)
   return false;
}

bool ProfileContentBackend::append(const Contact* item)
{
   Q_UNUSED(item)
   return false;
}

ProfileContentBackend::~ProfileContentBackend( )
{
   while (m_lProfiles.size()) {
      Node* c = m_lProfiles[0];
      m_lProfiles.removeAt(0);
      delete c;
   }
}

bool ProfileContentBackend::load()
{
   if (ProfilePersisterVisitor::instance()) {
      m_lProfiles.clear();

      QDir profilesDir = ProfilePersisterVisitor::instance()->getProfilesDir();

      qDebug() << "Loading vcf from:" << profilesDir;

      QStringList extensions = QStringList();
      extensions << "*.vcf";

      QStringList entries = profilesDir.entryList(extensions, QDir::Files);

      for (QString item : entries) {
         qDebug() << "Loading profile: " << item;
         QFile file(profilesDir.absolutePath()+"/"+item);
         if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "Error opening vcard: " << item;
            continue;
         }

         Contact* profile = new Contact(this);
         Node* pro = new Node;
         pro->type = Node::Type::PROFILE;
         pro->contact = profile;
         pro->m_Index = m_lProfiles.size();

         QByteArray all = file.readAll();
         QList<QByteArray> splittedProperties = all.split('\n');
         for (QByteArray property : splittedProperties){
            qDebug() << "property: " << property;
            QList<QByteArray> splitted = property.split(':');
            if(splitted.size() < 2){
               qDebug() << "Property malformed!";
               continue;
            }
            vc_mapper->metacall(profile,splitted[0],splitted[1].trimmed());

            //Link with accounts
            if(splitted.at(0) == VCardUtils::Property::X_RINGACCOUNT) {
               Account* acc = AccountListModel::instance()->getAccountById(splitted.at(1).trimmed());
               if(!acc) {
                  qDebug() << "Could not find account: " << splitted.at(1).trimmed();
                  continue;
               }

               Node* account_pro = new Node;
               account_pro->type = Node::Type::ACCOUNT;
               account_pro->contact = pro->contact;
               account_pro->parent = pro;
               account_pro->account = acc;
               account_pro->m_Index = pro->children.size();
               pro->children << account_pro;
               m_hProfileByAccountId[acc->id()] = pro;
            }
         }

         m_lProfiles << pro;
         connect(profile, SIGNAL(changed()), this, SLOT(contactChanged()));
         ContactModel::instance()->addContact(profile);
      }
   }
   else {
      qDebug() << "No ProfilePersister loaded!";
      return false;
   }
   return true;
}

bool ProfileContentBackend::reload()
{
   return false;
}

bool ProfileContentBackend::save(const Contact* contact)
{
   QDir profilesDir = ProfilePersisterVisitor::instance()->getProfilesDir();
   qDebug() << "Saving vcf in:" << profilesDir.absolutePath()+"/"+contact->uid()+".vcf";
   const QByteArray result = contact->toVCard(getAccountsForProfile(contact->uid()));

   QFile file(profilesDir.absolutePath()+"/"+contact->uid()+".vcf");
   file.open(QIODevice::WriteOnly);
   file.write(result);
   file.close();
   return true;
}

bool ProfileContentBackend::saveAll()
{
   for(Node* pro : m_lProfiles) {
      save(pro->contact);
   }
   return true;
}

ProfileContentBackend::SupportedFeatures ProfileContentBackend::supportedFeatures() const
{
   return (ProfileContentBackend::SupportedFeatures)(SupportedFeatures::NONE
      | SupportedFeatures::LOAD        //= 0x1 <<  0, /* Load this backend, DO NOT load anything before "load" is called         */
      | SupportedFeatures::EDIT        //= 0x1 <<  2, /* Edit, but **DOT NOT**, save an item)                                    */
      | SupportedFeatures::ADD         //= 0x1 <<  4, /* Add (and save) a new item to the backend                                */
      | SupportedFeatures::SAVE_ALL    //= 0x1 <<  5, /* Save all items at once, this may or may not be faster than "add"        */
      | SupportedFeatures::REMOVE      //= 0x1 <<  7, /* Remove a single item                                                    */
      | SupportedFeatures::ENABLEABLE  //= 0x1 << 10, /*Can be enabled, I know, it is not a word, but Java use it too            */
      | SupportedFeatures::MANAGEABLE);  //= 0x1 << 12, /* Can be managed the config GUI                                       */
      //TODO ^^ Remove that one once debugging is done
}

bool ProfileContentBackend::addPhoneNumber( Contact* contact , PhoneNumber* number)
{
   Q_UNUSED(contact)
   Q_UNUSED(number)
   return false;
}

QList<Contact*> ProfileContentBackend::items() const
{
   QList<Contact*> contacts;
   for (int var = 0; var < m_lProfiles.size(); ++var) {
      contacts << m_lProfiles[var]->contact;
   }
   return contacts;
}

QList<Account*> ProfileContentBackend::getAccountsForProfile(const QString& id)
{
   QList<Account*> result;
   Node* profile = getProfileById(id.toUtf8());
   if(!profile)
      return result;

   for (Node* child : profile->children) {
      result << child->account;
   }
   return result;
}

Node* ProfileContentBackend::getProfileById(const QByteArray& id)
{
   for (Node* p : m_lProfiles) {
      if(p->contact->uid() == id)
         return p;
   }
   return nullptr;
}

void ProfileContentBackend::contactChanged()
{
   Contact* c = qobject_cast<Contact*>(QObject::sender());
   qDebug() << c->formattedName();
   qDebug() << "contactChanged!";

   if(m_needSaving) {
      m_bSaveBuffer << c;
      QTimer::singleShot(0,this,SLOT(save()));
   }
   else
      m_needSaving = true;
}

void ProfileContentBackend::save()
{
   for (Contact* item : m_bSaveBuffer) {
      qDebug() << "saving:" << item->formattedName();
      save(item);
   }

   m_bSaveBuffer.clear();
   m_needSaving = false;
   load();
}

ProfileModel* ProfileModel::instance()
{
   if (!m_spInstance)
      m_spInstance = new ProfileModel();
   return m_spInstance;
}

ProfileModel::ProfileModel(QObject* parent) : QAbstractItemModel(parent)
{
   connect(AccountListModel::instance(),SIGNAL(dataChanged(QModelIndex,QModelIndex)),this,SLOT(slotDataChanged(QModelIndex,QModelIndex)));
   connect(AccountListModel::instance(),SIGNAL(layoutChanged()),this,SLOT(slotLayoutchanged()));

   m_lMimes << MIME_PLAIN_TEXT << MIME_HTML_TEXT << MIME_ACCOUNT << MIME_PROFILE;

   //Creating the profile contact backend
   m_pProfileBackend = new ProfileContentBackend(this);
   ContactModel::instance()->addBackend(m_pProfileBackend,LoadOptions::FORCE_ENABLED);

}

ProfileModel::~ProfileModel()
{
   delete m_pProfileBackend;
}

QModelIndex ProfileModel::mapToSource(const QModelIndex& idx) const
{
   if (!idx.isValid() || !idx.parent().isValid() || idx.model() != this)
      return QModelIndex();

   Node* profile = static_cast<Node*>(idx.parent().internalPointer());
   return profile->children[idx.row()]->account->index();
}

QModelIndex ProfileModel::mapFromSource(const QModelIndex& idx) const
{
   if (!idx.isValid() || idx.model() != AccountListModel::instance())
      return QModelIndex();

   Account* acc = AccountListModel::instance()->getAccountByModelIndex(idx);
   Node* pro = m_pProfileBackend->m_hProfileByAccountId[acc->id()];

   return AccountListModel::instance()->index(pro->m_Index,0,index(pro->parent->m_Index,0,QModelIndex()));
}

QVariant ProfileModel::data(const QModelIndex& index, int role ) const
{
   if (!index.isValid())
      return QVariant();
   else if (index.parent().isValid()) {
       switch (role) {
          case Qt::DisplayRole:
            return mapToSource(index).data();
       };
   }
   else {
      switch (role) {
         case Qt::DisplayRole:
            return m_pProfileBackend->items()[index.row()]->formattedName();
      };
   }
   return QVariant();
}

int ProfileModel::rowCount(const QModelIndex& parent ) const
{
   if (parent.parent().isValid())
      return 0;

   if (parent.isValid()) {
      // This is an account
      Node* account_node = static_cast<Node*>(parent.internalPointer());
      return account_node->children.size();
   }
   return m_pProfileBackend->items().size();
}

int ProfileModel::columnCount(const QModelIndex& parent ) const
{
   Q_UNUSED(parent)
   return 1;
}

QModelIndex ProfileModel::parent(const QModelIndex& idx ) const
{
   Node* current = static_cast<Node*>(idx.internalPointer());

   if (!current)
      return QModelIndex();
   switch (current->type) {
      case Node::Type::PROFILE:
         return QModelIndex();
      case Node::Type::ACCOUNT:
         return index(current->parent->m_Index, 0, QModelIndex());
   }
   return QModelIndex();
}

QModelIndex ProfileModel::index( int row, int column, const QModelIndex& parent ) const
{
   Node* current = static_cast<Node*>(parent.internalPointer());
   if(parent.isValid() && current && !column && row < current->children.size()) {
      return createIndex(row, 0, current->children[row]);
   }
   else if(row < m_pProfileBackend->m_lProfiles.size() && !column) {
      return createIndex(row, 0, m_pProfileBackend->m_lProfiles[row]);
   }
   return QModelIndex();
}

Qt::ItemFlags ProfileModel::flags(const QModelIndex& index ) const
{
   if (index.parent().isValid())
      return QAbstractItemModel::flags(index)
              | Qt::ItemIsUserCheckable
              | Qt::ItemIsEnabled
              | Qt::ItemIsSelectable
              | Qt::ItemIsDragEnabled
              | Qt::ItemIsDropEnabled;

   if(index.isValid())
       return QAbstractItemModel::flags(index)
               | Qt::ItemIsEnabled
               | Qt::ItemIsSelectable
               | Qt::ItemIsDragEnabled
               | Qt::ItemIsDropEnabled;

   return Qt::ItemIsEnabled;
}

QStringList ProfileModel::mimeType() const
{
   return m_lMimes;
}

QMimeData* ProfileModel::mimeData(const QModelIndexList &indexes) const
{
   QMimeData *mMimeData = new QMimeData();

   for (const QModelIndex &index : indexes) {
      Node* current = static_cast<Node*>(index.internalPointer());

      if (index.isValid() && index.parent().isValid() && current) {
         mMimeData->setData(MIME_ACCOUNT , current->account->id().toUtf8());
      }
      else if (index.isValid() && current) {
        mMimeData->setData(MIME_PROFILE , current->contact->uid());
      }
      else
         return nullptr;
   }
   return mMimeData;
}

///Return valid payload types
int ProfileModel::acceptedPayloadTypes() const
{
   return CallModel::DropPayloadType::PROFILE_ACCOUNT;
}

void ProfileModel::updateIndexes()
{
   for (int i = 0; i < m_pProfileBackend->m_lProfiles.size(); ++i) {
      m_pProfileBackend->m_lProfiles[i]->m_Index = i;
      for (int j = 0; j < m_pProfileBackend->m_lProfiles[i]->children.size(); ++j) {
         m_pProfileBackend->m_lProfiles[i]->children[j]->m_Index = j;
      }
   }
}

bool ProfileModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
   Q_UNUSED(action)
   if((parent.isValid() && row < 0) || column > 0) {
      qDebug() << "row or column invalid";
      return false;
   }

   if (data->hasFormat(MIME_ACCOUNT)) {
      qDebug() << "dropping account";

      QString accountId = data->data(MIME_ACCOUNT);
      Node* newProfile = nullptr;
      int destIdx, indexOfAccountToMove = -1; // Where to insert in account list of profile

      if(!parent.isValid() && row < m_pProfileBackend->m_lProfiles.size()) {
         qDebug() << "Dropping on profile title";
         qDebug() << "row:" << row;
         newProfile = m_pProfileBackend->m_lProfiles[row];
         destIdx = 0;
      }
      else if (parent.isValid()) {
         newProfile = static_cast<Node*>(parent.internalPointer());
         destIdx = row;
      }

      Node* accountProfile = m_pProfileBackend->m_hProfileByAccountId[accountId];
      for (Node* acc : accountProfile->children) {
         if(acc->account->id() == accountId) {
            indexOfAccountToMove = acc->m_Index;
            break;
         }
      }

      if(indexOfAccountToMove == -1) {
         qDebug() << "indexOfAccountToMove:" << indexOfAccountToMove;
         return false;
      }

      if(!beginMoveRows(index(accountProfile->m_Index, 0), indexOfAccountToMove, indexOfAccountToMove, parent, destIdx)) {
         return false;
      }

      Node* accountToMove = accountProfile->children.at(indexOfAccountToMove);
      qDebug() << "Moving:" << accountToMove->account->alias();
      accountProfile->children.remove(indexOfAccountToMove);
      accountToMove->parent = newProfile;
      m_pProfileBackend->m_hProfileByAccountId[accountId] = newProfile;
      newProfile->children.insert(destIdx, accountToMove);
      updateIndexes();
      m_pProfileBackend->saveAll();
      endMoveRows();
   }
   else if (data->hasFormat(MIME_PROFILE)) {
      qDebug() << "dropping profile";
      qDebug() << "row:" << row;

      int destinationRow = -1;
      if(row < 0) {
         // drop on bottom of the list
         destinationRow = m_pProfileBackend->m_lProfiles.size();
      }
      else {
         destinationRow = row;
      }

      Node* moving = m_pProfileBackend->getProfileById(data->data(MIME_PROFILE));

      if(!beginMoveRows(QModelIndex(), moving->m_Index, moving->m_Index, QModelIndex(), destinationRow)) {
         return false;
      }

      m_pProfileBackend->m_lProfiles.removeAt(moving->m_Index);
      m_pProfileBackend->m_lProfiles.insert(destinationRow, moving);
      updateIndexes();
      endMoveRows();

      return true;
   }
   return false;
}

bool ProfileModel::setData(const QModelIndex& index, const QVariant &value, int role )
{
   if (!index.isValid())
      return false;
   else if (index.parent().isValid()) {
      return AccountListModel::instance()->setData(mapToSource(index),value,role);
   }
   return false;
}

QVariant ProfileModel::headerData(int section, Qt::Orientation orientation, int role ) const
{
   Q_UNUSED(section)
   Q_UNUSED(orientation)
   if (role == Qt::DisplayRole) return tr("Profiles");
   return QVariant();
}

AbstractContactBackend* ProfileModel::getBackEnd()
{
   return m_pProfileBackend;
}

bool ProfileModel::addNewProfile(Contact* c, AbstractContactBackend* backend)
{
   Q_UNUSED(backend);
   return m_pProfileBackend->addNew(c);
}

void ProfileModel::slotDataChanged(const QModelIndex& tl,const QModelIndex& br)
{
   Q_UNUSED(tl)
   Q_UNUSED(br)
   emit layoutChanged();
}

void ProfileModel::slotLayoutchanged()
{
   emit layoutChanged();
}

#include "profilemodel.moc"
