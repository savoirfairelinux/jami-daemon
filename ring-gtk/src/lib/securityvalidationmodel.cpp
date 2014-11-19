/****************************************************************************
 *   Copyright (C) 2013-2014 by Savoir-Faire Linux                           *
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
#include "securityvalidationmodel.h"
#include "account.h"
#include "visitors/pixmapmanipulationvisitor.h"

#include <QtAlgorithms>

const QString SecurityValidationModel::messages[enum_class_size<SecurityFlaw>()] = {
   QObject::tr("Your communication negotiation is secured, but not the media stream, please enable ZRTP or SDES"),
   QObject::tr("TLS is disabled, the negotiation wont be encrypted. Your communication will be vulnerable to "
   "snooping"),
   QObject::tr("Your certificate is expired, please contact your system administrator."),
   QObject::tr("Your certificate is self signed. This break the chain of trust."),
   QObject::tr("CA_CERTIFICATE_MISSING         "),
   QObject::tr("END_CERTIFICATE_MISSING        "),
   QObject::tr("None of your certificate provide a private key, this is required. Please select a private key"
   " or use a certificate with one built-in"),
   QObject::tr("CERTIFICATE_MISMATCH           "),
   QObject::tr("CERTIFICATE_STORAGE_PERMISSION "),
   QObject::tr("CERTIFICATE_STORAGE_FOLDER     "),
   QObject::tr("CERTIFICATE_STORAGE_LOCATION   "),
   QObject::tr("OUTGOING_SERVER_MISMATCH       "),
   QObject::tr("VERIFY_INCOMING_DISABLED       "),
   QObject::tr("VERIFY_ANSWER_DISABLED         "),
   QObject::tr("REQUIRE_CERTIFICATE_DISABLED   "),
};

const TypedStateMachine< SecurityValidationModel::SecurityLevel , SecurityValidationModel::SecurityFlaw >
SecurityValidationModel::maximumSecurityLevel = {{
   /* SRTP_DISABLED                  */ SecurityLevel::WEAK       ,
   /* TLS_DISABLED                   */ SecurityLevel::WEAK       ,
   /* CERTIFICATE_EXPIRED            */ SecurityLevel::MEDIUM     ,
   /* CERTIFICATE_SELF_SIGNED        */ SecurityLevel::MEDIUM     ,
   /* CA_CERTIFICATE_MISSING         */ SecurityLevel::MEDIUM     ,
   /* END_CERTIFICATE_MISSING        */ SecurityLevel::MEDIUM     ,
   /* PRIVATE_KEY_MISSING            */ SecurityLevel::MEDIUM     ,
   /* CERTIFICATE_MISMATCH           */ SecurityLevel::NONE       ,
   /* CERTIFICATE_STORAGE_PERMISSION */ SecurityLevel::ACCEPTABLE ,
   /* CERTIFICATE_STORAGE_FOLDER     */ SecurityLevel::ACCEPTABLE ,
   /* CERTIFICATE_STORAGE_LOCATION   */ SecurityLevel::ACCEPTABLE ,
   /* OUTGOING_SERVER_MISMATCH       */ SecurityLevel::ACCEPTABLE ,
   /* VERIFY_INCOMING_DISABLED       */ SecurityLevel::MEDIUM     ,
   /* VERIFY_ANSWER_DISABLED         */ SecurityLevel::MEDIUM     ,
   /* REQUIRE_CERTIFICATE_DISABLED   */ SecurityLevel::MEDIUM     ,
   /* MISSING_CERTIFICATE            */ SecurityLevel::NONE       ,
   /* MISSING_AUTHORITY              */ SecurityLevel::WEAK       ,
}};

const TypedStateMachine< SecurityValidationModel::Severity , SecurityValidationModel::SecurityFlaw >
SecurityValidationModel::flawSeverity = {{
   /* SRTP_DISABLED                  */ Severity::ISSUE   ,
   /* TLS_DISABLED                   */ Severity::ISSUE   ,
   /* CERTIFICATE_EXPIRED            */ Severity::WARNING ,
   /* CERTIFICATE_SELF_SIGNED        */ Severity::WARNING ,
   /* CA_CERTIFICATE_MISSING         */ Severity::ISSUE   ,
   /* END_CERTIFICATE_MISSING        */ Severity::ISSUE   ,
   /* PRIVATE_KEY_MISSING            */ Severity::ERROR   ,
   /* CERTIFICATE_MISMATCH           */ Severity::ERROR   ,
   /* CERTIFICATE_STORAGE_PERMISSION */ Severity::WARNING ,
   /* CERTIFICATE_STORAGE_FOLDER     */ Severity::INFORMATION ,
   /* CERTIFICATE_STORAGE_LOCATION   */ Severity::INFORMATION ,
   /* OUTGOING_SERVER_MISMATCH       */ Severity::WARNING ,
   /* VERIFY_INCOMING_DISABLED       */ Severity::ISSUE   ,
   /* VERIFY_ANSWER_DISABLED         */ Severity::ISSUE   ,
   /* REQUIRE_CERTIFICATE_DISABLED   */ Severity::ISSUE   ,
   /* MISSING_CERTIFICATE            */ Severity::ERROR   ,
   /* MISSING_AUTHORITY              */ Severity::ERROR   ,
}};


SecurityValidationModel::SecurityValidationModel(Account* account) : QAbstractListModel(account),
m_pAccount(account),m_CurrentSecurityLevel(SecurityLevel::NONE)
{

}

SecurityValidationModel::~SecurityValidationModel()
{

}

QVariant SecurityValidationModel::data( const QModelIndex& index, int role) const
{
   if (index.isValid())  {
      if (role == Qt::DisplayRole) {
         return messages[static_cast<int>( m_lCurrentFlaws[index.row()]->flaw() )];
      }
      else if (role == Role::SeverityRole) {
         return static_cast<int>(m_lCurrentFlaws[index.row()]->severity());
      }
      else if (role == Qt::DecorationRole) {
         return PixmapManipulationVisitor::instance()->serurityIssueIcon(index);
      }
   }
   return QVariant();
}

int SecurityValidationModel::rowCount( const QModelIndex& parent) const
{
   Q_UNUSED(parent)
   return m_lCurrentFlaws.size();
}

Qt::ItemFlags SecurityValidationModel::flags( const QModelIndex& index) const
{
   if (!index.isValid()) return Qt::NoItemFlags;
   return Qt::ItemIsEnabled|Qt::ItemIsSelectable;
}

bool SecurityValidationModel::setData( const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED(index)
   Q_UNUSED(value)
   Q_UNUSED(role )
   return false;
}

///Do some unsafe convertions to bypass Qt4 issues with C++11 enum class
Flaw* SecurityValidationModel::getFlaw(SecurityFlaw _se,Certificate::Type _ty)
{
   if (! m_hFlaws[(int)_se][(int)_ty]) {
      m_hFlaws[(int)_se][(int)_ty] = new Flaw(_se,_ty);
   }
   return m_hFlaws[(int)_se][(int)_ty];
}

#define _F(_se,_ty) getFlaw(SecurityFlaw::_se,_ty);
void SecurityValidationModel::update()
{
   m_lCurrentFlaws.clear();

   /**********************************
    *     Check general issues       *
    *********************************/

   /* If TLS is not enabled, everything else is worthless */
   if (!m_pAccount->isTlsEnabled()) {
      m_lCurrentFlaws << _F(TLS_DISABLED,Certificate::Type::NONE);
   }

   /* Check if the media stream is encrypted, it is something users
    * may care about if they get this far ;) */
   if (!m_pAccount->isSrtpEnabled()) {
      m_lCurrentFlaws << _F(SRTP_DISABLED,Certificate::Type::NONE);
   }

   /* The user certificate need to have a private key, otherwise it wont
    * be possible to encrypt anything */
   if ((! m_pAccount->tlsCertificate()->hasPrivateKey()) && (!m_pAccount->tlsPrivateKeyCertificate()->exist())) {
      m_lCurrentFlaws << _F(PRIVATE_KEY_MISSING,m_pAccount->tlsPrivateKeyCertificate()->type());
   }

   /**********************************
    *      Certificates issues       *
    *********************************/
   QList<Certificate*> certs;
   certs << m_pAccount->tlsCaListCertificate() << m_pAccount->tlsCertificate() << m_pAccount->tlsPrivateKeyCertificate();
   foreach (Certificate* cert, certs) {
      if (! cert->exist()) {
         m_lCurrentFlaws << _F(END_CERTIFICATE_MISSING,cert->type());
      }
      if (! cert->isExpired()) {
         m_lCurrentFlaws << _F(CERTIFICATE_EXPIRED,cert->type());
      }
      if (! cert->isSelfSigned()) {
         m_lCurrentFlaws << _F(CERTIFICATE_SELF_SIGNED,cert->type());
      }
      if (! cert->hasProtectedPrivateKey()) {
         m_lCurrentFlaws << _F(CERTIFICATE_STORAGE_PERMISSION,cert->type());
      }
      if (! cert->hasRightPermissions()) {
         m_lCurrentFlaws << _F(CERTIFICATE_STORAGE_PERMISSION,cert->type());
      }
      if (! cert->hasRightFolderPermissions()) {
         m_lCurrentFlaws << _F(CERTIFICATE_STORAGE_FOLDER,cert->type());
      }
      if (! cert->isLocationSecure()) {
         m_lCurrentFlaws << _F(CERTIFICATE_STORAGE_LOCATION,cert->type());
      }
   }

   qSort(m_lCurrentFlaws.begin(),m_lCurrentFlaws.end(),[] (const Flaw* f1, const Flaw* f2) -> int {
      return (*f1) < (*f2);
   });
   for (int i=0;i<m_lCurrentFlaws.size();i++) {
      m_lCurrentFlaws[i]->m_Row = i;
   }

   emit layoutChanged();
}
#undef _F

QModelIndex SecurityValidationModel::getIndex(const Flaw* flaw)
{
   return index(flaw->m_Row,0);
}

QList<Flaw*> SecurityValidationModel::currentFlaws()
{
   return m_lCurrentFlaws;
}

Certificate::Type Flaw::type() const
{
   return m_certType;
}

SecurityValidationModel::SecurityFlaw Flaw::flaw() const
{
   return m_flaw;
}

SecurityValidationModel::Severity Flaw::severity() const
{
   return m_severity;
}

void Flaw::slotRequestHighlight()
{
   emit requestHighlight();
}
