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
#ifndef PHONENUMBER_H
#define PHONENUMBER_H

#include "typedefs.h"
#include <time.h>

//Qt
#include <QStringList>
#include <QtCore/QSize>
#include <QtCore/QObject>

//SFLPhone
#include "uri.h"
class Account;
class Contact;
class Call;
class PhoneNumberPrivate;
class TemporaryPhoneNumber;
class NumberCategory;

class PrivatePhoneNumber;

///PhoneNumber: represent a phone number
class LIB_EXPORT PhoneNumber : public QObject {
   Q_OBJECT
public:
   friend class PhoneDirectoryModel;
   friend class PrivatePhoneNumber;
   virtual ~PhoneNumber();

   //Properties
   Q_PROPERTY(Account*      account         READ account  WRITE setAccount)
   Q_PROPERTY(Contact*      contact         READ contact  WRITE setContact)
   Q_PROPERTY(int           lastUsed        READ lastUsed                 )
   Q_PROPERTY(QString       uri             READ uri                      )
   Q_PROPERTY(int           callCount       READ callCount                )
   Q_PROPERTY(QList<Call*>  calls           READ calls                    )
   Q_PROPERTY(int           popularityIndex READ popularityIndex          )
   Q_PROPERTY(bool          bookmarked      READ isBookmarked             )
   Q_PROPERTY(QString       uid             READ uid      WRITE setUid    )
   Q_PROPERTY(bool               isTracked        READ isTracked         NOTIFY trackedChanged       )
   Q_PROPERTY(bool               isPresent        READ isPresent         NOTIFY presentChanged       )
   Q_PROPERTY(bool               supportPresence  READ supportPresence          )
   Q_PROPERTY(QString            presenceMessage  READ presenceMessage  NOTIFY presenceMessageChanged        )
   Q_PROPERTY(uint               weekCount        READ weekCount                )
   Q_PROPERTY(uint               trimCount        READ trimCount                )
   Q_PROPERTY(bool               haveCalled       READ haveCalled               )
   Q_PROPERTY(QString            primaryName      READ primaryName              )
   Q_PROPERTY(bool               isBookmarked     READ isBookmarked             )
   Q_PROPERTY(QVariant           icon             READ icon                     )
   Q_PROPERTY(int                totalSpentTime   READ totalSpentTime           )

//    Q_PROPERTY(QHash<QString,int> alternativeNames READ alternativeNames         )

   ///@enum Type: Is this temporary, blank, used or unused
   enum class Type {
      BLANK     = 0, /*This number represent no number                                  */
      TEMPORARY = 1, /*This number is not yet complete                                  */
      USED      = 2, /*This number have been called before                              */
      UNUSED    = 3, /*This number have never been called, but is in the address book   */
      ACCOUNT   = 4, /*This number correspond to the URI of a SIP account               */
   };
   Q_ENUMS(Type)

   //Getters
   URI                uri             () const;
   NumberCategory*    category        () const;
   bool               isTracked       () const;
   bool               isPresent       () const;
   QString            presenceMessage () const;
   Account*           account         () const;
   Contact*           contact         () const;
   time_t             lastUsed        () const;
   PhoneNumber::Type  type            () const;
   int                callCount       () const;
   uint               weekCount       () const;
   uint               trimCount       () const;
   bool               haveCalled      () const;
   QList<Call*>       calls           () const;
   int                popularityIndex () const;
   QHash<QString,int> alternativeNames() const;
   QString            primaryName     () const;
   bool               isBookmarked    () const;
   bool               supportPresence () const;
   QVariant           icon            () const;
   int                totalSpentTime  () const;
   QString            uid             () const;

   //Setters
   Q_INVOKABLE void setAccount(Account*       account);
   Q_INVOKABLE void setContact(Contact*       contact);
   Q_INVOKABLE void setTracked(bool           track  );
   void             setCategory(NumberCategory* cat  );
   void             setBookmarked(bool bookmarked    );
   void             setUid(const QString& uri        );
   bool             setType(PhoneNumber::Type t      );

   //Mutator
   Q_INVOKABLE void addCall(Call* call);
   Q_INVOKABLE void incrementAlternativeName(const QString& name);

   //Static
   static const PhoneNumber* BLANK();

   //Helper
   QString toHash() const;

   //Operator
   bool operator==(PhoneNumber* other);
   bool operator==(const PhoneNumber* other) const;
   bool operator==(PhoneNumber& other);
   bool operator==(const PhoneNumber& other) const;

protected:
   //Constructor
   PhoneNumber(const URI& uri, NumberCategory* cat, Type st = Type::UNUSED);

   //Private setters
   void setPresent(bool present);
   void setPresenceMessage(const QString& message);

   //PhoneDirectoryModel mutator
   bool merge(PhoneNumber* other);

   //Getter
   bool hasType() const;
   int  index() const;

   //Setter
   void setHasType(bool value);
   void setIndex(int value);
   void setPopularityIndex(int value);

   //Many phone numbers can have the same "d" if they were merged
   PrivatePhoneNumber* d;

private:
   friend class PhoneNumberPrivate;

   /*//Attributes
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
   QString            m_PrimaryName_cache;*/

   //Static attributes
   static QHash<int,Call*> m_shMostUsed  ;
   static const PhoneNumber* m_spBlank   ;

private Q_SLOTS:
   void accountDestroyed(QObject* o);
   void contactRebased(Contact* other);

Q_SIGNALS:
   void callAdded(Call* call);
   void changed  (          );
   void presentChanged(bool);
   void presenceMessageChanged(const QString&);
   void trackedChanged(bool);
   void primaryNameChanged(const QString& name);
   void rebased(PhoneNumber* other);
};

Q_DECLARE_METATYPE(PhoneNumber*)

///@class TemporaryPhoneNumber: An incomplete phone number
class LIB_EXPORT TemporaryPhoneNumber : public PhoneNumber {
   Q_OBJECT
public:
   explicit TemporaryPhoneNumber(const PhoneNumber* number = nullptr);
   void setUri(const QString& uri);
};

#endif
