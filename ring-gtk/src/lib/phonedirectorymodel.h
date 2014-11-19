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
#ifndef PHONEDIRECTORYMODEL_H
#define PHONEDIRECTORYMODEL_H
#include "typedefs.h"

//Qt
#include <QtCore/QString>
#include <QtCore/QAbstractTableModel>

//SFLPhone
#include "uri.h"
class PhoneNumber         ;
class Contact             ;
class Account             ;
class Call                ;
class TemporaryPhoneNumber;

///CredentialModel: A model for account credentials
class LIB_EXPORT PhoneDirectoryModel : public QAbstractTableModel {

   //NumberCompletionModel need direct access to the indexes
   friend class NumberCompletionModel;

   //Friend unit test class
   friend class AutoCompletionTest;

   //Phone number need to update the indexes as they change
   friend class PhoneNumber          ;

   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
public:
   Q_PROPERTY(int count READ count )

   virtual ~PhoneDirectoryModel();

   //Abstract model members
   virtual QVariant      data       (const QModelIndex& index, int role = Qt::DisplayRole                 ) const;
   virtual int           rowCount   (const QModelIndex& parent = QModelIndex()                            ) const;
   virtual int           columnCount(const QModelIndex& parent = QModelIndex()                            ) const;
   virtual Qt::ItemFlags flags      (const QModelIndex& index                                             ) const;
   virtual bool          setData    (const QModelIndex& index, const QVariant &value, int role            )      ;
   virtual QVariant      headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;

   //Singleton
   static PhoneDirectoryModel* instance();

   //Factory
   Q_INVOKABLE PhoneNumber* getNumber(const QString& uri, const QString& type = QString());
   Q_INVOKABLE PhoneNumber* getNumber(const QString& uri, Account* account, const QString& type = QString());
   Q_INVOKABLE PhoneNumber* getNumber(const QString& uri, Contact* contact, Account* account = nullptr, const QString& type = QString());
   Q_INVOKABLE PhoneNumber* fromHash(const QString& hash);
   Q_INVOKABLE PhoneNumber* fromTemporary(const TemporaryPhoneNumber* number);

   //Getter
   int count() const;
   bool callWithAccount() const;

   //Setters
   void setCallWithAccount(bool value);

   //Static
   QVector<PhoneNumber*> getNumbersByPopularity() const;

protected:
   //Internal data structures
   ///@struct NumberWrapper Wrap phone numbers to prevent collisions
   struct NumberWrapper {
      QVector<PhoneNumber*> numbers;
   };

private:

   //Model columns
   enum class Columns {
      URI              = 0,
      TYPE             = 1,
      CONTACT          = 2,
      ACCOUNT          = 3,
      STATE            = 4,
      CALL_COUNT       = 5,
      WEEK_COUNT       = 6,
      TRIM_COUNT       = 7,
      HAVE_CALLED      = 8,
      LAST_USED        = 9,
      NAME_COUNT       = 10,
      TOTAL_SECONDS    = 11,
      POPULARITY_INDEX = 12,
      BOOKMARED        = 13,
      TRACKED          = 14,
      PRESENT          = 15,
      PRESENCE_MESSAGE = 16,
      UID              = 17,
   };

   //Constructor
   explicit PhoneDirectoryModel(QObject* parent = nullptr);

   //Helpers
   void indexNumber(PhoneNumber* number, const QStringList& names   );
   void setAccount (PhoneNumber* number,       Account*     account );
   PhoneNumber* fillDetails(NumberWrapper* wrap, const URI& strippedUri, Account* account, Contact* contact, const QString& type);

   //Singleton
   static PhoneDirectoryModel* m_spInstance;

   //Attributes
   QVector<PhoneNumber*>         m_lNumbers         ;
   QHash<QString,NumberWrapper*> m_hDirectory       ;
   QVector<PhoneNumber*>         m_lPopularityIndex ;
   QMap<QString,NumberWrapper*>  m_lSortedNames     ;
   QMap<QString,NumberWrapper*>  m_hSortedNumbers   ;
   QHash<QString,NumberWrapper*> m_hNumbersByNames  ;
   bool                          m_CallWithAccount  ;

private Q_SLOTS:
   void slotCallAdded(Call* call);
   void slotChanged();

   //From DBus
   void slotNewBuddySubscription(const QString& uri, const QString& accountId, bool status, const QString& message);
//    void slotStatusChanges(const QString& accountId, const QString& uri, bool status); //Deprecated?
};
Q_DECLARE_METATYPE(PhoneDirectoryModel*)

#endif
