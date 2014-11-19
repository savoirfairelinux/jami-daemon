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
#ifndef SECURITYVALIDATIONMODEL_H
#define SECURITYVALIDATIONMODEL_H
#include <QAbstractListModel>

//SFLPhone
#include "certificate.h"
#include "typedefs.h"


//SFLPhone
class Account;
class Flaw;

class LIB_EXPORT SecurityValidationModel : public QAbstractListModel {
   Q_OBJECT
   friend class Flaw;
public:
   /*
    * This class evaluate the overall security of an account.
    * It does so by checking various potential flaws, then create
    * a metric called SecurityLevel. This model should be used to:
    *
    * 1) List all potential flaws
    * 2) Decide if an account can be considered secure
    * 3) Decide if a call can be considered secure
    *
    * End users should not have to be security gurus to setup SFLphone. It is our
    * job to do as much as we can to make security configuration as transparent as
    * possible.
    *
    * The SecurityLevel is computed by checking all possible flaw. The level cannot be
    * higher than a flaw maximum security level. If there is 2 (or more) flaw in the same
    * maximum level, the maximum level will be decreased by one (recursively).
    *
    * A flaw severity is used by the client to display the right icon ( (i), /!\, [x] ).
    */

   ///Give the user an overview of the current security state
   enum class SecurityLevel {
      NONE        = 0, /* Security is not functional or severely defective              */
      WEAK        = 1, /* There is some security, but way too many flaws                */
      MEDIUM      = 2, /* The security is probably good enough, but there is issues     */
      ACCEPTABLE  = 3, /* The security is most probably good enough, only minor issues  */
      STRONG      = 4, /* All the non-information items are correct                     */
      COMPLETE    = 5, /* Everything, even the recommendations, are correct             */
   };

   ///The severity of a given flaw
   enum class Severity {
      INFORMATION  , /* Tip and tricks to have better security                          */
      WARNING      , /* It is a problem, but it wont have other side effects            */
      ISSUE        , /* The security is compromised                                     */
      ERROR        , /* It simply wont work (REGISTER)                                  */
      FATAL_WARNING, /* Registration may work, but it render everything else useless    */
   };

   ///Every supported flaws
   enum class SecurityFlaw {
      SRTP_DISABLED                  ,
      TLS_DISABLED                   ,
      CERTIFICATE_EXPIRED            ,
      CERTIFICATE_SELF_SIGNED        ,
      CA_CERTIFICATE_MISSING         ,
      END_CERTIFICATE_MISSING        ,
      PRIVATE_KEY_MISSING            ,
      CERTIFICATE_MISMATCH           ,
      CERTIFICATE_STORAGE_PERMISSION ,
      CERTIFICATE_STORAGE_FOLDER     ,
      CERTIFICATE_STORAGE_LOCATION   ,
      OUTGOING_SERVER_MISMATCH       ,
      VERIFY_INCOMING_DISABLED       ,
      VERIFY_ANSWER_DISABLED         ,
      REQUIRE_CERTIFICATE_DISABLED   ,
      MISSING_CERTIFICATE            ,
      MISSING_AUTHORITY              ,
      __COUNT
   };

   ///Role for the model
   enum Role {
      SeverityRole = 100
   };

   ///Messages to show to the end user
   static const QString messages[enum_class_size<SecurityFlaw>()];

   //Constructor
   explicit SecurityValidationModel(Account* account);
   virtual ~SecurityValidationModel();


   //Model functions
   QVariant      data     ( const QModelIndex& index, int role = Qt::DisplayRole     ) const;
   int           rowCount ( const QModelIndex& parent = QModelIndex()                ) const;
   Qt::ItemFlags flags    ( const QModelIndex& index                                 ) const;
   virtual bool  setData  ( const QModelIndex& index, const QVariant &value, int role)      ;

   //Getter
   QList<Flaw*> currentFlaws();
   QModelIndex getIndex(const Flaw* flaw);

   //Mutator
   void update();

private:
   //Attributes
   QList<Flaw*>  m_lCurrentFlaws       ;
   SecurityLevel m_CurrentSecurityLevel;
   Account*      m_pAccount            ;
   QHash< int, QHash< int, Flaw* > > m_hFlaws;

   //Helpers
   Flaw* getFlaw(SecurityFlaw _se,Certificate::Type _ty);

   //Static mapping
   static const TypedStateMachine< SecurityLevel , SecurityFlaw > maximumSecurityLevel;
   static const TypedStateMachine< Severity      , SecurityFlaw > flawSeverity        ;
};
Q_DECLARE_METATYPE(SecurityValidationModel*)

///A flaw representation
class LIB_EXPORT Flaw : public QObject
{
   Q_OBJECT
   friend class SecurityValidationModel;
public:

   //Operators
   bool operator < ( const Flaw &r ) const {
      return ( (int)m_severity > (int)r.m_severity );
   }
   bool operator > ( const Flaw &r ) const {
      return ( (int)m_severity < (int)r.m_severity );
   }

   //Getter
   Certificate::Type type() const;
   SecurityValidationModel::SecurityFlaw flaw() const;
   SecurityValidationModel::Severity severity() const;
private:
   //Constructor
   Flaw(SecurityValidationModel::SecurityFlaw f,Certificate::Type type = Certificate::Type::NONE)
   : m_flaw(f),m_certType(type),m_Row(-1)
   {
      m_severity = SecurityValidationModel::flawSeverity[f];
   }

   //Attributes
   SecurityValidationModel::SecurityFlaw m_flaw;
   SecurityValidationModel::Severity m_severity;
   Certificate::Type m_certType;
   int m_Row;
public Q_SLOTS:
   void slotRequestHighlight();

Q_SIGNALS:
   void solved();
   void requestHighlight();
};

#endif
