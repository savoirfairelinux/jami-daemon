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
#include "account.h"

//Qt
#include <QtCore/QDebug>
#include <QtCore/QObject>
#include <QtCore/QString>

//SFLPhone
#include "sflphone_const.h"

//SFLPhone lib
#include "dbus/configurationmanager.h"
#include "dbus/callmanager.h"
#include "dbus/videomanager.h"
#include "visitors/accountlistcolorvisitor.h"
#include "certificate.h"
#include "accountlistmodel.h"
#include "credentialmodel.h"
#include "audiocodecmodel.h"
#include "video/videocodecmodel.h"
#include "ringtonemodel.h"
#include "phonenumber.h"
#include "phonedirectorymodel.h"
#include "presencestatusmodel.h"
#include "profilemodel.h"
#include "uri.h"
#include "securityvalidationmodel.h"
#define TO_BOOL ?"true":"false"
#define IS_TRUE == "true"

const account_function Account::stateMachineActionsOnState[6][7] = {
/*                 NOTHING              EDIT              RELOAD              SAVE               REMOVE             MODIFY             CANCEL            */
/*READY    */{ &Account::nothing, &Account::edit   , &Account::reload , &Account::nothing, &Account::remove , &Account::modify   , &Account::nothing },/**/
/*EDITING  */{ &Account::nothing, &Account::nothing, &Account::outdate, &Account::nothing, &Account::remove , &Account::modify   , &Account::cancel  },/**/
/*OUTDATED */{ &Account::nothing, &Account::nothing, &Account::nothing, &Account::nothing, &Account::remove , &Account::reloadMod, &Account::reload  },/**/
/*NEW      */{ &Account::nothing, &Account::nothing, &Account::nothing, &Account::save   , &Account::remove , &Account::nothing  , &Account::nothing },/**/
/*MODIFIED */{ &Account::nothing, &Account::nothing, &Account::nothing, &Account::save   , &Account::remove , &Account::nothing  , &Account::reload  },/**/
/*REMOVED  */{ &Account::nothing, &Account::nothing, &Account::nothing, &Account::nothing, &Account::nothing, &Account::nothing  , &Account::cancel  } /**/
/*                                                                                                                                                       */
};

///Constructors
Account::Account():QObject(AccountListModel::instance()),m_pCredentials(nullptr),m_pAudioCodecs(nullptr),m_CurrentState(AccountEditState::READY),
m_pVideoCodecs(nullptr),m_LastErrorCode(-1),m_VoiceMailCount(0),m_pRingToneModel(nullptr),m_pAccountNumber(nullptr),
m_pKeyExchangeModel(nullptr),m_pSecurityValidationModel(nullptr),m_pCaCert(nullptr),m_pTlsCert(nullptr),m_pPrivateKey(nullptr)
{
}

///Build an account from it'id
Account* Account::buildExistingAccountFromId(const QString& _accountId)
{
//    qDebug() << "Building an account from id: " << _accountId;
   Account* a = new Account();
   a->m_AccountId = _accountId;
   a->setObjectName(_accountId);

   a->performAction(AccountEditAction::RELOAD);

   return a;
} //buildExistingAccountFromId

///Build an account from it's name / alias
Account* Account::buildNewAccountFromAlias(const QString& alias)
{
   qDebug() << "Building an account from alias: " << alias;
   ConfigurationManagerInterface& configurationManager = DBus::ConfigurationManager::instance();
   Account* a = new Account();
   a->m_hAccountDetails.clear();
   a->m_hAccountDetails[Account::MapField::ENABLED] = "false";
   a->m_pAccountNumber = const_cast<PhoneNumber*>(PhoneNumber::BLANK());
   MapStringString tmp = configurationManager.getAccountTemplate();
   QMutableMapIterator<QString, QString> iter(tmp);
   while (iter.hasNext()) {
      iter.next();
      a->m_hAccountDetails[iter.key()] = iter.value();
   }
   a->setHostname(a->m_hAccountDetails[Account::MapField::HOSTNAME]);
   a->setAccountDetail(Account::MapField::ALIAS,alias);
   a->setObjectName(a->id());
   return a;
}

///Destructor
Account::~Account()
{
   disconnect();
   if (m_pCredentials) delete m_pCredentials ;
   if (m_pAudioCodecs) delete m_pAudioCodecs ;
}


/*****************************************************************************
 *                                                                           *
 *                                   Slots                                   *
 *                                                                           *
 ****************************************************************************/

///Callback when the account state change
// void Account::accountChanged(const QString& accountId, const QString& state,int)
// {
//    if ((!m_AccountId.isEmpty()) && accountId == m_AccountId) {
//       if (state != "OK") //Do not polute the log
//          qDebug() << "Account" << m_AccountId << "status changed to" << state;
//       if (Account::updateState())
//          emit stateChanged(toHumanStateName());
//    }
// }

void Account::slotPresentChanged(bool present)
{
   Q_UNUSED(present)
   emit changed(this);
}

void Account::slotPresenceMessageChanged(const QString& message)
{
   Q_UNUSED(message)
   emit changed(this);
}

void Account::slotUpdateCertificate()
{
   Certificate* cert = qobject_cast<Certificate*>(sender());
   if (cert) {
      switch (cert->type()) {
         case Certificate::Type::AUTHORITY:
            if (accountDetail(Account::MapField::TLS::CA_LIST_FILE) != cert->path().toString())
               setAccountDetail(Account::MapField::TLS::CA_LIST_FILE, cert->path().toString());
            break;
         case Certificate::Type::USER:
            if (accountDetail(Account::MapField::TLS::CERTIFICATE_FILE) != cert->path().toString())
               setAccountDetail(Account::MapField::TLS::CERTIFICATE_FILE, cert->path().toString());
            break;
         case Certificate::Type::PRIVATE_KEY:
            if (accountDetail(Account::MapField::TLS::PRIVATE_KEY_FILE) != cert->path().toString())
               setAccountDetail(Account::MapField::TLS::PRIVATE_KEY_FILE, cert->path().toString());
            break;
         case Certificate::Type::NONE:
            break;
      };
   }
}

/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///IS this account new
bool Account::isNew() const
{
   return (m_AccountId == nullptr) || m_AccountId.isEmpty();
}

///Get this account ID
const QString Account::id() const
{
   if (isNew()) {
      qDebug() << "Error : getting AccountId of a new account.";
   }
   if (m_AccountId.isEmpty()) {
      qDebug() << "Account not configured";
      return QString(); //WARNING May explode
   }

   return m_AccountId;
}

///Get current state
const QString Account::toHumanStateName() const
{
   const QString s = m_hAccountDetails[Account::MapField::Registration::STATUS];

   static const QString registered             = tr("Registered"               );
   static const QString notRegistered          = tr("Not Registered"           );
   static const QString trying                 = tr("Trying..."                );
   static const QString error                  = tr("Error"                    );
   static const QString authenticationFailed   = tr("Authentication Failed"    );
   static const QString networkUnreachable     = tr("Network unreachable"      );
   static const QString hostUnreachable        = tr("Host unreachable"         );
   static const QString stunConfigurationError = tr("Stun configuration error" );
   static const QString stunServerInvalid      = tr("Stun server invalid"      );
   static const QString serviceUnavailable     = tr("Service unavailable"      );
   static const QString notAcceptable          = tr("Unacceptable"             );
   static const QString invalid                = tr("Invalid"                  );
   static const QString requestTimeout         = tr("Request Timeout"          );

   if(s == Account::State::REGISTERED       )
      return registered             ;
   if(s == Account::State::UNREGISTERED     )
      return notRegistered          ;
   if(s == Account::State::TRYING           )
      return trying                 ;
   if(s == Account::State::ERROR            )
      return m_LastErrorMessage.isEmpty()?error:m_LastErrorMessage;
   if(s == Account::State::ERROR_AUTH       )
      return authenticationFailed   ;
   if(s == Account::State::ERROR_NETWORK    )
      return networkUnreachable     ;
   if(s == Account::State::ERROR_HOST       )
      return hostUnreachable        ;
   if(s == Account::State::ERROR_CONF_STUN  )
      return stunConfigurationError ;
   if(s == Account::State::ERROR_EXIST_STUN )
      return stunServerInvalid      ;
   if(s == Account::State::ERROR_SERVICE_UNAVAILABLE )
      return serviceUnavailable     ;
   if(s == Account::State::ERROR_NOT_ACCEPTABLE      )
      return notAcceptable          ;
   if(s == Account::State::REQUEST_TIMEOUT           )
      return requestTimeout         ;
   return invalid                   ;
}

///Get an account detail
const QString Account::accountDetail(const QString& param) const
{
   if (!m_hAccountDetails.size()) {
      qDebug() << "The account details is not set";
      return QString(); //May crash, but better than crashing now
   }
   if (m_hAccountDetails.find(param) != m_hAccountDetails.end()) {
      return m_hAccountDetails[param];
   }
   else if (m_hAccountDetails.count() > 0) {
      if (param == Account::MapField::ENABLED) //If an account is invalid, at least does not try to register it
         return Account::RegistrationEnabled::NO;
      if (param == Account::MapField::Registration::STATUS) //If an account is new, then it is unregistered
         return Account::State::UNREGISTERED;
      if (protocol() != Account::Protocol::IAX) //IAX accounts lack some fields, be quiet
         qDebug() << "Account parameter \"" << param << "\" not found";
      return QString();
   }
   else {
      qDebug() << "Account details not found, there is " << m_hAccountDetails.count() << " details available";
      return QString();
   }
} //accountDetail

///Get the alias
const QString Account::alias() const
{
   return accountDetail(Account::MapField::ALIAS);
}

///Is this account registered
bool Account::isRegistered() const
{
   return (accountDetail(Account::MapField::Registration::STATUS) == Account::State::REGISTERED);
}

///Return the model index of this item
QModelIndex Account::index()
{
   for (int i=0;i < AccountListModel::instance()->m_lAccounts.size();i++) {
      if (this == (AccountListModel::instance()->m_lAccounts)[i]) {

         return AccountListModel::instance()->index(i,0);
      }
   }
   return QModelIndex();
}

///Return status color name
QString Account::stateColorName() const
{
   if(registrationStatus() == Account::State::UNREGISTERED)
      return "black";
   if(registrationStatus() == Account::State::REGISTERED || registrationStatus() == Account::State::READY)
      return "darkGreen";
   return "red";
}

///Return status Qt color, QColor is not part of QtCore, use using the global variant
QVariant Account::stateColor() const
{
   if (AccountListModel::instance()->colorVisitor()) {
      return AccountListModel::instance()->colorVisitor()->getColor(this);
   }
   return QVariant();
}

///Create and return the credential model
CredentialModel* Account::credentialsModel() const
{
   if (!m_pCredentials)
      const_cast<Account*>(this)->reloadCredentials();
   return m_pCredentials;
}

///Create and return the audio codec model
AudioCodecModel* Account::audioCodecModel() const
{
   if (!m_pAudioCodecs)
      const_cast<Account*>(this)->reloadAudioCodecs();
   return m_pAudioCodecs;
}

///Create and return the video codec model
VideoCodecModel* Account::videoCodecModel() const
{
   if (!m_pVideoCodecs)
      const_cast<Account*>(this)->m_pVideoCodecs = new VideoCodecModel(const_cast<Account*>(this));
   return m_pVideoCodecs;
}

RingToneModel* Account::ringToneModel() const
{
   if (!m_pRingToneModel)
      const_cast<Account*>(this)->m_pRingToneModel = new RingToneModel(const_cast<Account*>(this));
   return m_pRingToneModel;
}

KeyExchangeModel* Account::keyExchangeModel() const
{
   if (!m_pKeyExchangeModel) {
      const_cast<Account*>(this)->m_pKeyExchangeModel = new KeyExchangeModel(const_cast<Account*>(this));
   }
   return m_pKeyExchangeModel;
}

SecurityValidationModel* Account::securityValidationModel() const
{
   if (!m_pSecurityValidationModel) {
      const_cast<Account*>(this)->m_pSecurityValidationModel = new SecurityValidationModel(const_cast<Account*>(this));
   }
   return m_pSecurityValidationModel;
}

void Account::setAlias(const QString& detail)
{
   bool accChanged = detail != alias();
   setAccountDetail(Account::MapField::ALIAS,detail);
   if (accChanged)
      emit aliasChanged(detail);
}

///Return the account hostname
QString Account::hostname() const
{
   return m_HostName;
}

///Return if the account is enabled
bool Account::isEnabled() const
{
   return accountDetail(Account::MapField::ENABLED) IS_TRUE;
}

///Return if the account should auto answer
bool Account::isAutoAnswer() const
{
   return accountDetail(Account::MapField::AUTOANSWER) IS_TRUE;
}

///Return the account user name
QString Account::username() const
{
   return accountDetail(Account::MapField::USERNAME);
}

///Return the account mailbox address
QString Account::mailbox() const
{
   return accountDetail(Account::MapField::MAILBOX);
}

///Return the account mailbox address
QString Account::proxy() const
{
   return accountDetail(Account::MapField::ROUTE);
}


QString Account::password() const
{
   switch (protocol()) {
      case Account::Protocol::SIP:
         if (credentialsModel()->rowCount())
            return credentialsModel()->data(credentialsModel()->index(0,0),CredentialModel::Role::PASSWORD).toString();
      case Account::Protocol::IAX:
         return accountDetail(Account::MapField::PASSWORD);
   };
   return "";
}

///
bool Account::isDisplaySasOnce() const
{
   return accountDetail(Account::MapField::ZRTP::DISPLAY_SAS_ONCE) IS_TRUE;
}

///Return the account security fallback
bool Account::isSrtpRtpFallback() const
{
   return accountDetail(Account::MapField::SRTP::RTP_FALLBACK) IS_TRUE;
}

//Return if SRTP is enabled or not
bool Account::isSrtpEnabled() const
{
   return accountDetail(Account::MapField::SRTP::ENABLED) IS_TRUE;
}

///
bool Account::isZrtpDisplaySas         () const
{
   return accountDetail(Account::MapField::ZRTP::DISPLAY_SAS) IS_TRUE;
}

///Return if the other side support warning
bool Account::isZrtpNotSuppWarning() const
{
   return accountDetail(Account::MapField::ZRTP::NOT_SUPP_WARNING) IS_TRUE;
}

///
bool Account::isZrtpHelloHash() const
{
   return accountDetail(Account::MapField::ZRTP::HELLO_HASH) IS_TRUE;
}

///Return if the account is using a STUN server
bool Account::isSipStunEnabled() const
{
   return accountDetail(Account::MapField::STUN::ENABLED) IS_TRUE;
}

///Return the account STUN server
QString Account::sipStunServer() const
{
   return accountDetail(Account::MapField::STUN::SERVER);
}

///Return when the account expire (require renewal)
int Account::registrationExpire() const
{
   return accountDetail(Account::MapField::Registration::EXPIRE).toInt();
}

///Return if the published address is the same as the local one
bool Account::isPublishedSameAsLocal() const
{
   return accountDetail(Account::MapField::PUBLISHED_SAMEAS_LOCAL) IS_TRUE;
}

///Return the account published address
QString Account::publishedAddress() const
{
   return accountDetail(Account::MapField::PUBLISHED_ADDRESS);
}

///Return the account published port
int Account::publishedPort() const
{
   return accountDetail(Account::MapField::PUBLISHED_PORT).toUInt();
}

///Return the account tls password
QString Account::tlsPassword() const
{
   return accountDetail(Account::MapField::TLS::PASSWORD);
}

///Return the account TLS port
int Account::tlsListenerPort() const
{
   return accountDetail(Account::MapField::TLS::LISTENER_PORT).toInt();
}

///Return the account TLS certificate authority list file
Certificate* Account::tlsCaListCertificate() const
{
   if (!m_pCaCert) {
      const_cast<Account*>(this)->m_pCaCert = new Certificate(Certificate::Type::AUTHORITY,this);
      connect(m_pCaCert,SIGNAL(changed()),this,SLOT(slotUpdateCertificate()));
   }
   const_cast<Account*>(this)->m_pCaCert->setPath(accountDetail(Account::MapField::TLS::CA_LIST_FILE));
   return m_pCaCert;
}

///Return the account TLS certificate
Certificate* Account::tlsCertificate() const
{
   if (!m_pTlsCert) {
      const_cast<Account*>(this)->m_pTlsCert = new Certificate(Certificate::Type::USER,this);
      connect(m_pTlsCert,SIGNAL(changed()),this,SLOT(slotUpdateCertificate()));
   }
   const_cast<Account*>(this)->m_pTlsCert->setPath(accountDetail(Account::MapField::TLS::CERTIFICATE_FILE));
   return m_pTlsCert;
}

///Return the account private key
Certificate* Account::tlsPrivateKeyCertificate() const
{
   if (!m_pPrivateKey) {
      const_cast<Account*>(this)->m_pPrivateKey = new Certificate(Certificate::Type::PRIVATE_KEY,this);
      connect(m_pPrivateKey,SIGNAL(changed()),this,SLOT(slotUpdateCertificate()));
   }
   const_cast<Account*>(this)->m_pPrivateKey->setPath(accountDetail(Account::MapField::TLS::PRIVATE_KEY_FILE));
   return m_pPrivateKey;
}

///Return the account cipher
QString Account::tlsCiphers() const
{
   return accountDetail(Account::MapField::TLS::CIPHERS);
}

///Return the account TLS server name
QString Account::tlsServerName() const
{
   return accountDetail(Account::MapField::TLS::SERVER_NAME);
}

///Return the account negotiation timeout in seconds
int Account::tlsNegotiationTimeoutSec() const
{
   return accountDetail(Account::MapField::TLS::NEGOTIATION_TIMEOUT_SEC).toInt();
}

///Return the account negotiation timeout in milliseconds
int Account::tlsNegotiationTimeoutMsec() const
{
   return accountDetail(Account::MapField::TLS::NEGOTIATION_TIMEOUT_MSEC).toInt();
}

///Return the account TLS verify server
bool Account::isTlsVerifyServer() const
{
   return (accountDetail(Account::MapField::TLS::VERIFY_SERVER) IS_TRUE);
}

///Return the account TLS verify client
bool Account::isTlsVerifyClient() const
{
   return (accountDetail(Account::MapField::TLS::VERIFY_CLIENT) IS_TRUE);
}

///Return if it is required for the peer to have a certificate
bool Account::isTlsRequireClientCertificate() const
{
   return (accountDetail(Account::MapField::TLS::REQUIRE_CLIENT_CERTIFICATE) IS_TRUE);
}

///Return the account TLS security is enabled
bool Account::isTlsEnabled() const
{
   return (accountDetail(Account::MapField::TLS::ENABLED) IS_TRUE);
}

///Return the account the TLS encryption method
TlsMethodModel::Type Account::tlsMethod() const
{
   const QString value = accountDetail(Account::MapField::TLS::METHOD);
   return TlsMethodModel::fromDaemonName(value);
}

///Return the key exchange mechanism
KeyExchangeModel::Type Account::keyExchange() const
{
   return KeyExchangeModel::fromDaemonName(accountDetail(Account::MapField::SRTP::KEY_EXCHANGE));
}

///Return if the ringtone are enabled
bool Account::isRingtoneEnabled() const
{
   return (accountDetail(Account::MapField::Ringtone::ENABLED) IS_TRUE);
}

///Return the account ringtone path
QString Account::ringtonePath() const
{
   return accountDetail(Account::MapField::Ringtone::PATH);
}

///Return the last error message received
QString Account::lastErrorMessage() const
{
   return m_LastErrorMessage;
}

///Return the last error code (useful for debugging)
int Account::lastErrorCode() const
{
   return m_LastErrorCode;
}

///Return the account local port
int Account::localPort() const
{
   return accountDetail(Account::MapField::LOCAL_PORT).toInt();
}

///Return the number of voicemails
int Account::voiceMailCount() const
{
   return m_VoiceMailCount;
}

///Return the account local interface
QString Account::localInterface() const
{
   return accountDetail(Account::MapField::LOCAL_INTERFACE);
}

///Return the account registration status
QString Account::registrationStatus() const
{
   return accountDetail(Account::MapField::Registration::STATUS);
}

///Return the account type
Account::Protocol Account::protocol() const
{
   const QString str = accountDetail(Account::MapField::TYPE);
   if (str.isEmpty() || str == Account::ProtocolName::SIP)
      return Account::Protocol::SIP;
   else if (str == Account::ProtocolName::IAX)
      return Account::Protocol::IAX;
   qDebug() << "Warning: unhandled protocol name" << str << ", defaulting to SIP";
   return Account::Protocol::SIP;
}

///Return the DTMF type
DtmfType Account::DTMFType() const
{
   QString type = accountDetail(Account::MapField::DTMF_TYPE);
   return (type == "overrtp" || type.isEmpty())? DtmfType::OverRtp:DtmfType::OverSip;
}

bool Account::presenceStatus() const
{
   return m_pAccountNumber->isPresent();
}

QString Account::presenceMessage() const
{
   return m_pAccountNumber->presenceMessage();
}

bool Account::supportPresencePublish() const
{
   return accountDetail(Account::MapField::Presence::SUPPORT_PUBLISH) IS_TRUE;
}

bool Account::supportPresenceSubscribe() const
{
   return accountDetail(Account::MapField::Presence::SUPPORT_SUBSCRIBE) IS_TRUE;
}

bool Account::presenceEnabled() const
{
   return accountDetail(Account::MapField::Presence::ENABLED) IS_TRUE;
}

bool Account::isVideoEnabled() const
{
   return accountDetail(Account::MapField::Video::ENABLED) IS_TRUE;
}

int Account::videoPortMax() const
{
   return accountDetail(Account::MapField::Video::PORT_MAX).toInt();
}

int Account::videoPortMin() const
{
   return accountDetail(Account::MapField::Video::PORT_MIN).toInt();
}

int Account::audioPortMin() const
{
   return accountDetail(Account::MapField::Audio::PORT_MIN).toInt();
}

int Account::audioPortMax() const
{
   return accountDetail(Account::MapField::Audio::PORT_MAX).toInt();
}

QString Account::userAgent() const
{
   return accountDetail(Account::MapField::USER_AGENT);
}

QVariant Account::roleData(int role) const
{
   switch(role) {
      case Account::Role::Alias:
         return alias();
      case Account::Role::Proto:
         return static_cast<int>(protocol());
      case Account::Role::Hostname:
         return hostname();
      case Account::Role::Username:
         return username();
      case Account::Role::Mailbox:
         return mailbox();
      case Account::Role::Proxy:
         return proxy();
//       case Password:
//          return accountPassword();
      case Account::Role::TlsPassword:
         return tlsPassword();
      case Account::Role::TlsCaListCertificate:
         return tlsCaListCertificate()->path().toLocalFile();
      case Account::Role::TlsCertificate:
         return tlsCertificate()->path().toLocalFile();
      case Account::Role::TlsPrivateKeyCertificate:
         return tlsPrivateKeyCertificate()->path().toLocalFile();
      case Account::Role::TlsCiphers:
         return tlsCiphers();
      case Account::Role::TlsServerName:
         return tlsServerName();
      case Account::Role::SipStunServer:
         return sipStunServer();
      case Account::Role::PublishedAddress:
         return publishedAddress();
      case Account::Role::LocalInterface:
         return localInterface();
      case Account::Role::RingtonePath:
         return ringtonePath();
      case Account::Role::TlsMethod:
         return static_cast<int>(tlsMethod());
      case Account::Role::RegistrationExpire:
         return registrationExpire();
      case Account::Role::TlsNegotiationTimeoutSec:
         return tlsNegotiationTimeoutSec();
      case Account::Role::TlsNegotiationTimeoutMsec:
         return tlsNegotiationTimeoutMsec();
      case Account::Role::LocalPort:
         return localPort();
      case Account::Role::TlsListenerPort:
         return tlsListenerPort();
      case Account::Role::PublishedPort:
         return publishedPort();
      case Account::Role::Enabled:
         return isEnabled();
      case Account::Role::AutoAnswer:
         return isAutoAnswer();
      case Account::Role::TlsVerifyServer:
         return isTlsVerifyServer();
      case Account::Role::TlsVerifyClient:
         return isTlsVerifyClient();
      case Account::Role::TlsRequireClientCertificate:
         return isTlsRequireClientCertificate();
      case Account::Role::TlsEnabled:
         return isTlsEnabled();
      case Account::Role::DisplaySasOnce:
         return isDisplaySasOnce();
      case Account::Role::SrtpRtpFallback:
         return isSrtpRtpFallback();
      case Account::Role::ZrtpDisplaySas:
         return isZrtpDisplaySas();
      case Account::Role::ZrtpNotSuppWarning:
         return isZrtpNotSuppWarning();
      case Account::Role::ZrtpHelloHash:
         return isZrtpHelloHash();
      case Account::Role::SipStunEnabled:
         return isSipStunEnabled();
      case Account::Role::PublishedSameAsLocal:
         return isPublishedSameAsLocal();
      case Account::Role::RingtoneEnabled:
         return isRingtoneEnabled();
      case Account::Role::dTMFType:
         return DTMFType();
      case Account::Role::Id:
         return id();
      case Account::Role::Object: {
         QVariant var;
         var.setValue(const_cast<Account*>(this));
         return var;
      }
      case Account::Role::TypeName:
         return static_cast<int>(protocol());
      case Account::Role::PresenceStatus:
         return PresenceStatusModel::instance()->currentStatus();
      case Account::Role::PresenceMessage:
         return PresenceStatusModel::instance()->currentMessage();
      default:
         return QVariant();
   }
}


/*****************************************************************************
 *                                                                           *
 *                                  Setters                                  *
 *                                                                           *
 ****************************************************************************/

///Set account details
void Account::setAccountDetails(const QHash<QString,QString>& m)
{
   m_hAccountDetails.clear();
   m_hAccountDetails = m;
   m_HostName = m[Account::MapField::HOSTNAME];
}

///Set a specific detail
bool Account::setAccountDetail(const QString& param, const QString& val)
{
   const bool accChanged = m_hAccountDetails[param] != val;
   const QString buf = m_hAccountDetails[param];
   if (param == Account::MapField::Registration::STATUS) {
      m_hAccountDetails[param] = val;
      if (accChanged) {
         emit detailChanged(this,param,val,buf);
      }
   }
   else {
      performAction(AccountEditAction::MODIFY);
      if (m_CurrentState == AccountEditState::MODIFIED || m_CurrentState == AccountEditState::NEW) {
         m_hAccountDetails[param] = val;
         if (accChanged) {
            emit detailChanged(this,param,val,buf);
         }
      }
   }
   return m_CurrentState == AccountEditState::MODIFIED || m_CurrentState == AccountEditState::NEW;
}

///Set the account id
void Account::setId(const QString& id)
{
   qDebug() << "Setting accountId = " << m_AccountId;
   if (! isNew())
      qDebug() << "Error : setting AccountId of an existing account.";
   m_AccountId = id;
}

///Set the account type, SIP or IAX
void Account::setProtocol(Account::Protocol proto)
{
   switch (proto) {
      case Account::Protocol::SIP:
         setAccountDetail(Account::MapField::TYPE ,Account::ProtocolName::SIP);
         break;
      case Account::Protocol::IAX:
         setAccountDetail(Account::MapField::TYPE ,Account::ProtocolName::IAX);
         break;
   };
}

///The set account hostname, it can be an hostname or an IP address
void Account::setHostname(const QString& detail)
{
   if (m_HostName != detail) {
      m_HostName = detail;
      setAccountDetail(Account::MapField::HOSTNAME, detail);
   }
}

///Set the account username, everything is valid, some might be rejected by the PBX server
void Account::setUsername(const QString& detail)
{
   setAccountDetail(Account::MapField::USERNAME, detail);
}

///Set the account mailbox, usually a number, but can be anything
void Account::setMailbox(const QString& detail)
{
   setAccountDetail(Account::MapField::MAILBOX, detail);
}

///Set the account mailbox, usually a number, but can be anything
void Account::setProxy(const QString& detail)
{
   setAccountDetail(Account::MapField::ROUTE, detail);
}

///Set the main credential password
void Account::setPassword(const QString& detail)
{
   switch (protocol()) {
      case Account::Protocol::SIP:
         if (credentialsModel()->rowCount())
            credentialsModel()->setData(credentialsModel()->index(0,0),detail,CredentialModel::Role::PASSWORD);
         else {
            const QModelIndex idx = credentialsModel()->addCredentials();
            credentialsModel()->setData(idx,detail,CredentialModel::Role::PASSWORD);
         }
         break;
      case Account::Protocol::IAX:
         setAccountDetail(Account::MapField::PASSWORD, detail);
         break;
   };
}

///Set the TLS (encryption) password
void Account::setTlsPassword(const QString& detail)
{
   setAccountDetail(Account::MapField::TLS::PASSWORD, detail);
}

///Set the certificate authority list file
void Account::setTlsCaListCertificate(Certificate* cert)
{
   m_pCaCert = cert; //FIXME memory leak
   setAccountDetail(Account::MapField::TLS::CA_LIST_FILE, cert?cert->path().toLocalFile():QString());
}

///Set the certificate
void Account::setTlsCertificate(Certificate* cert)
{
   m_pTlsCert = cert; //FIXME memory leak
   setAccountDetail(Account::MapField::TLS::CERTIFICATE_FILE, cert?cert->path().toLocalFile():QString());
}

///Set the private key
void Account::setTlsPrivateKeyCertificate(Certificate* cert)
{
   m_pPrivateKey = cert; //FIXME memory leak
   setAccountDetail(Account::MapField::TLS::PRIVATE_KEY_FILE, cert?cert->path().toLocalFile():QString());
}

///Set the TLS cipher
void Account::setTlsCiphers(const QString& detail)
{
   setAccountDetail(Account::MapField::TLS::CIPHERS, detail);
}

///Set the TLS server
void Account::setTlsServerName(const QString& detail)
{
   setAccountDetail(Account::MapField::TLS::SERVER_NAME, detail);
}

///Set the stun server
void Account::setSipStunServer(const QString& detail)
{
   setAccountDetail(Account::MapField::STUN::SERVER, detail);
}

///Set the published address
void Account::setPublishedAddress(const QString& detail)
{
   setAccountDetail(Account::MapField::PUBLISHED_ADDRESS, detail);
}

///Set the local interface
void Account::setLocalInterface(const QString& detail)
{
   setAccountDetail(Account::MapField::LOCAL_INTERFACE, detail);
}

///Set the ringtone path, it have to be a valid absolute path
void Account::setRingtonePath(const QString& detail)
{
   setAccountDetail(Account::MapField::Ringtone::PATH, detail);
}

///Set the number of voice mails
void Account::setVoiceMailCount(int count)
{
   m_VoiceMailCount = count;
}

///Set the last error message to be displayed as status instead of "Error"
void Account::setLastErrorMessage(const QString& message)
{
   m_LastErrorMessage = message;
}

///Set the last error code
void Account::setLastErrorCode(int code)
{
   m_LastErrorCode = code;
}

///Set the Tls method
void Account::setTlsMethod(TlsMethodModel::Type detail)
{

   setAccountDetail(Account::MapField::TLS::METHOD ,TlsMethodModel::toDaemonName(detail));
}

///Set the Tls method
void Account::setKeyExchange(KeyExchangeModel::Type detail)
{
   setAccountDetail(Account::MapField::SRTP::KEY_EXCHANGE ,KeyExchangeModel::toDaemonName(detail));
}

///Set the account timeout, it will be renegotiated when that timeout occur
void Account::setRegistrationExpire(int detail)
{
   setAccountDetail(Account::MapField::Registration::EXPIRE, QString::number(detail));
}

///Set TLS negotiation timeout in second
void Account::setTlsNegotiationTimeoutSec(int detail)
{
   setAccountDetail(Account::MapField::TLS::NEGOTIATION_TIMEOUT_SEC, QString::number(detail));
}

///Set the TLS negotiation timeout in milliseconds
void Account::setTlsNegotiationTimeoutMsec(int detail)
{
   setAccountDetail(Account::MapField::TLS::NEGOTIATION_TIMEOUT_MSEC, QString::number(detail));
}

///Set the local port for SIP/IAX communications
void Account::setLocalPort(unsigned short detail)
{
   setAccountDetail(Account::MapField::LOCAL_PORT, QString::number(detail));
}

///Set the TLS listener port (0-2^16)
void Account::setTlsListenerPort(unsigned short detail)
{
   setAccountDetail(Account::MapField::TLS::LISTENER_PORT, QString::number(detail));
}

///Set the published port (0-2^16)
void Account::setPublishedPort(unsigned short detail)
{
   setAccountDetail(Account::MapField::PUBLISHED_PORT, QString::number(detail));
}

///Set if the account is enabled or not
void Account::setEnabled(bool detail)
{
   setAccountDetail(Account::MapField::ENABLED, (detail)TO_BOOL);
}

///Set if the account should auto answer
void Account::setAutoAnswer(bool detail)
{
   setAccountDetail(Account::MapField::AUTOANSWER, (detail)TO_BOOL);
}

///Set the TLS verification server
void Account::setTlsVerifyServer(bool detail)
{
   setAccountDetail(Account::MapField::TLS::VERIFY_SERVER, (detail)TO_BOOL);
}

///Set the TLS verification client
void Account::setTlsVerifyClient(bool detail)
{
   setAccountDetail(Account::MapField::TLS::VERIFY_CLIENT, (detail)TO_BOOL);
}

///Set if the peer need to be providing a certificate
void Account::setTlsRequireClientCertificate(bool detail)
{
   setAccountDetail(Account::MapField::TLS::REQUIRE_CLIENT_CERTIFICATE ,(detail)TO_BOOL);
}

///Set if the security settings are enabled
void Account::setTlsEnabled(bool detail)
{
   setAccountDetail(Account::MapField::TLS::ENABLED ,(detail)TO_BOOL);
}

void Account::setDisplaySasOnce(bool detail)
{
   setAccountDetail(Account::MapField::ZRTP::DISPLAY_SAS_ONCE, (detail)TO_BOOL);
}

void Account::setSrtpRtpFallback(bool detail)
{
   setAccountDetail(Account::MapField::SRTP::RTP_FALLBACK, (detail)TO_BOOL);
}

void Account::setSrtpEnabled(bool detail)
{
   setAccountDetail(Account::MapField::SRTP::ENABLED, (detail)TO_BOOL);
}

void Account::setZrtpDisplaySas(bool detail)
{
   setAccountDetail(Account::MapField::ZRTP::DISPLAY_SAS, (detail)TO_BOOL);
}

void Account::setZrtpNotSuppWarning(bool detail)
{
   setAccountDetail(Account::MapField::ZRTP::NOT_SUPP_WARNING, (detail)TO_BOOL);
}

void Account::setZrtpHelloHash(bool detail)
{
   setAccountDetail(Account::MapField::ZRTP::HELLO_HASH, (detail)TO_BOOL);
}

void Account::setSipStunEnabled(bool detail)
{
   setAccountDetail(Account::MapField::STUN::ENABLED, (detail)TO_BOOL);
}

void Account::setPublishedSameAsLocal(bool detail)
{
   setAccountDetail(Account::MapField::PUBLISHED_SAMEAS_LOCAL, (detail)TO_BOOL);
}

///Set if custom ringtone are enabled
void Account::setRingtoneEnabled(bool detail)
{
   setAccountDetail(Account::MapField::Ringtone::ENABLED, (detail)TO_BOOL);
}

void Account::setPresenceEnabled(bool enable)
{
   setAccountDetail(Account::MapField::Presence::ENABLED, (enable)TO_BOOL);
   emit presenceEnabledChanged(enable);
}

///Use video by default when available
void Account::setVideoEnabled(bool enable)
{
   setAccountDetail(Account::MapField::Video::ENABLED, (enable)TO_BOOL);
}

void Account::setAudioPortMax(int port )
{
   setAccountDetail(Account::MapField::Audio::PORT_MAX, QString::number(port));
}

void Account::setAudioPortMin(int port )
{
   setAccountDetail(Account::MapField::Audio::PORT_MIN, QString::number(port));
}

void Account::setVideoPortMax(int port )
{
   setAccountDetail(Account::MapField::Video::PORT_MAX, QString::number(port));
}

void Account::setVideoPortMin(int port )
{
   setAccountDetail(Account::MapField::Video::PORT_MIN, QString::number(port));
}

void Account::setUserAgent(const QString& agent)
{
   setAccountDetail(Account::MapField::USER_AGENT, agent);
}

///Set the DTMF type
void Account::setDTMFType(DtmfType type)
{
   setAccountDetail(Account::MapField::DTMF_TYPE,(type==OverRtp)?"overrtp":"oversip");
}

void Account::setRoleData(int role, const QVariant& value)
{
   switch(role) {
      case Account::Role::Alias:
         setAlias(value.toString());
         break;
      case Account::Role::Proto: {
         const int proto = value.toInt();
         setProtocol((proto>=0&&proto<=1)?static_cast<Account::Protocol>(proto):Account::Protocol::SIP);
         break;
      }
      case Account::Role::Hostname:
         setHostname(value.toString());
         break;
      case Account::Role::Username:
         setUsername(value.toString());
         break;
      case Account::Role::Mailbox:
         setMailbox(value.toString());
         break;
      case Account::Role::Proxy:
         setProxy(value.toString());
         break;
//       case Password:
//          accountPassword();
      case Account::Role::TlsPassword:
         setTlsPassword(value.toString());
         break;
      case Account::Role::TlsCaListCertificate: {
         const QString path = value.toString();
         if ((tlsCaListCertificate() && tlsCaListCertificate()->path() != QUrl(path)) || !tlsCaListCertificate()) {
            tlsCaListCertificate()->setPath(path);
         }
         break;
      }
      case Account::Role::TlsCertificate: {
         const QString path = value.toString();
         if ((tlsCertificate() && tlsCertificate()->path() != QUrl(path)) || !tlsCertificate())
            tlsCertificate()->setPath(path);
      }
         break;
      case Account::Role::TlsPrivateKeyCertificate: {
         const QString path = value.toString();
         if ((tlsPrivateKeyCertificate() && tlsPrivateKeyCertificate()->path() != QUrl(path)) || !tlsPrivateKeyCertificate())
            tlsPrivateKeyCertificate()->setPath(path);
      }
         break;
      case Account::Role::TlsCiphers:
         setTlsCiphers(value.toString());
         break;
      case Account::Role::TlsServerName:
         setTlsServerName(value.toString());
         break;
      case Account::Role::SipStunServer:
         setSipStunServer(value.toString());
         break;
      case Account::Role::PublishedAddress:
         setPublishedAddress(value.toString());
         break;
      case Account::Role::LocalInterface:
         setLocalInterface(value.toString());
         break;
      case Account::Role::RingtonePath:
         setRingtonePath(value.toString());
         break;
      case Account::Role::TlsMethod: {
         const int method = value.toInt();
         setTlsMethod(method<=TlsMethodModel::instance()->rowCount()?static_cast<TlsMethodModel::Type>(method):TlsMethodModel::Type::DEFAULT);
         break;
      }
      case Account::Role::KeyExchange: {
         const int method = value.toInt();
         setKeyExchange(method<=keyExchangeModel()->rowCount()?static_cast<KeyExchangeModel::Type>(method):KeyExchangeModel::Type::NONE);
         break;
      }
      case Account::Role::RegistrationExpire:
         setRegistrationExpire(value.toInt());
         break;
      case Account::Role::TlsNegotiationTimeoutSec:
         setTlsNegotiationTimeoutSec(value.toInt());
         break;
      case Account::Role::TlsNegotiationTimeoutMsec:
         setTlsNegotiationTimeoutMsec(value.toInt());
         break;
      case Account::Role::LocalPort:
         setLocalPort(value.toInt());
         break;
      case Account::Role::TlsListenerPort:
         setTlsListenerPort(value.toInt());
         break;
      case Account::Role::PublishedPort:
         setPublishedPort(value.toInt());
         break;
      case Account::Role::Enabled:
         setEnabled(value.toBool());
         break;
      case Account::Role::AutoAnswer:
         setAutoAnswer(value.toBool());
         break;
      case Account::Role::TlsVerifyServer:
         setTlsVerifyServer(value.toBool());
         break;
      case Account::Role::TlsVerifyClient:
         setTlsVerifyClient(value.toBool());
         break;
      case Account::Role::TlsRequireClientCertificate:
         setTlsRequireClientCertificate(value.toBool());
         break;
      case Account::Role::TlsEnabled:
         setTlsEnabled(value.toBool());
         break;
      case Account::Role::DisplaySasOnce:
         setDisplaySasOnce(value.toBool());
         break;
      case Account::Role::SrtpRtpFallback:
         setSrtpRtpFallback(value.toBool());
         break;
      case Account::Role::ZrtpDisplaySas:
         setZrtpDisplaySas(value.toBool());
         break;
      case Account::Role::ZrtpNotSuppWarning:
         setZrtpNotSuppWarning(value.toBool());
         break;
      case Account::Role::ZrtpHelloHash:
         setZrtpHelloHash(value.toBool());
         break;
      case Account::Role::SipStunEnabled:
         setSipStunEnabled(value.toBool());
         break;
      case Account::Role::PublishedSameAsLocal:
         setPublishedSameAsLocal(value.toBool());
         break;
      case Account::Role::RingtoneEnabled:
         setRingtoneEnabled(value.toBool());
         break;
      case Account::Role::dTMFType:
         setDTMFType((DtmfType)value.toInt());
         break;
      case Account::Role::Id:
         setId(value.toString());
         break;
   }
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

bool Account::performAction(AccountEditAction action)
{
   AccountEditState curState = m_CurrentState;
   (this->*(stateMachineActionsOnState[(int)m_CurrentState][(int)action]))();//FIXME don't use integer cast
   return curState != m_CurrentState;
}

Account::AccountEditState Account::state() const
{
   return m_CurrentState;
}

/**Update the account
 * @return if the state changed
 */
bool Account::updateState()
{
   if(! isNew()) {
      ConfigurationManagerInterface & configurationManager = DBus::ConfigurationManager::instance();
      const MapStringString details       = configurationManager.getAccountDetails(id()).value();
      const QString         status        = details[Account::MapField::Registration::STATUS];
      const QString         currentStatus = registrationStatus();
      setAccountDetail(Account::MapField::Registration::STATUS, status); //Update -internal- object state
      return status == currentStatus;
   }
   return true;
}

///Save the current account to the daemon
void Account::save()
{
   ConfigurationManagerInterface& configurationManager = DBus::ConfigurationManager::instance();
   if (isNew()) {
      MapStringString details;
      QMutableHashIterator<QString,QString> iter(m_hAccountDetails);

      while (iter.hasNext()) {
         iter.next();
         details[iter.key()] = iter.value();
      }

      const QString currentId = configurationManager.addAccount(details);

      //Be sure there is audio codec enabled to avoid obscure error messages for the user
      const QVector<int> codecIdList = configurationManager.getAudioCodecList();
      foreach (const int aCodec, codecIdList) {
         const QStringList codec = configurationManager.getAudioCodecDetails(aCodec);
         const QModelIndex idx = m_pAudioCodecs->addAudioCodec();
         m_pAudioCodecs->setData(idx,codec[0],AudioCodecModel::Role::NAME       );
         m_pAudioCodecs->setData(idx,codec[1],AudioCodecModel::Role::SAMPLERATE );
         m_pAudioCodecs->setData(idx,codec[2],AudioCodecModel::Role::BITRATE    );
         m_pAudioCodecs->setData(idx,aCodec  ,AudioCodecModel::Role::ID         );
         m_pAudioCodecs->setData(idx, Qt::Checked ,Qt::CheckStateRole);
      }
      saveAudioCodecs();

      setId(currentId);
      saveCredentials();
   } //New account
   else { //Existing account
      MapStringString tmp;
      QMutableHashIterator<QString,QString> iter(m_hAccountDetails);

      while (iter.hasNext()) {
         iter.next();
         tmp[iter.key()] = iter.value();
      }
      configurationManager.setAccountDetails(id(), tmp);
   }

   if (!id().isEmpty()) {
      Account* acc =  AccountListModel::instance()->getAccountById(id());
      qDebug() << "Adding the new account to the account list (" << id() << ")";
      if (acc != this) {
         (AccountListModel::instance()->m_lAccounts) << this;
      }

      performAction(AccountEditAction::RELOAD);
      updateState();
      m_CurrentState = AccountEditState::READY;
   }
   #ifdef ENABLE_VIDEO
   videoCodecModel()->save();
   #endif
   saveAudioCodecs();
   emit changed(this);
}

///sync with the daemon, this need to be done manually to prevent reloading the account while it is being edited
void Account::reload()
{
   if (!isNew()) {
      if (m_hAccountDetails.size())
         qDebug() << "Reloading" << id() << alias();
      else
         qDebug() << "Loading" << id();
      ConfigurationManagerInterface& configurationManager = DBus::ConfigurationManager::instance();
      QMap<QString,QString> aDetails = configurationManager.getAccountDetails(id());

      if (!aDetails.count()) {
         qDebug() << "Account not found";
      }
      else {
         m_hAccountDetails.clear();
         QMutableMapIterator<QString, QString> iter(aDetails);
         while (iter.hasNext()) {
            iter.next();
            m_hAccountDetails[iter.key()] = iter.value();
         }
         setHostname(m_hAccountDetails[Account::MapField::HOSTNAME]);
      }
      m_CurrentState = AccountEditState::READY;

      const QString currentUri = QString("%1@%2").arg(username()).arg(m_HostName);
      if (!m_pAccountNumber || (m_pAccountNumber && m_pAccountNumber->uri() != currentUri)) {
         if (m_pAccountNumber) {
            disconnect(m_pAccountNumber,SIGNAL(presenceMessageChanged(QString)),this,SLOT(slotPresenceMessageChanged(QString)));
            disconnect(m_pAccountNumber,SIGNAL(presentChanged(bool)),this,SLOT(slotPresentChanged(bool)));
         }
         m_pAccountNumber = PhoneDirectoryModel::instance()->getNumber(currentUri,this );
         m_pAccountNumber->setType(PhoneNumber::Type::ACCOUNT);
         connect(m_pAccountNumber,SIGNAL(presenceMessageChanged(QString)),this,SLOT(slotPresenceMessageChanged(QString)));
         connect(m_pAccountNumber,SIGNAL(presentChanged(bool)),this,SLOT(slotPresentChanged(bool)));
      }

      //If the credential model is loaded, then update it
      if (m_pCredentials)
         reloadCredentials();
      emit changed(this);
   }
}

///Reload credentials from DBUS
void Account::reloadCredentials()
{
   if (!m_pCredentials) {
      m_pCredentials = new CredentialModel(this);
   }
   if (!isNew()) {
      m_pCredentials->clear();
      ConfigurationManagerInterface& configurationManager = DBus::ConfigurationManager::instance();
      VectorMapStringString credentials = configurationManager.getCredentials(id());
      for (int i=0; i < credentials.size(); i++) {
         QModelIndex idx = m_pCredentials->addCredentials();
         m_pCredentials->setData(idx,credentials[i][ Account::MapField::USERNAME ],CredentialModel::Role::NAME    );
         m_pCredentials->setData(idx,credentials[i][ Account::MapField::PASSWORD ],CredentialModel::Role::PASSWORD);
         m_pCredentials->setData(idx,credentials[i][ Account::MapField::REALM    ],CredentialModel::Role::REALM   );
      }
   }
}

///Save all credentials
void Account::saveCredentials() {
   if (m_pCredentials) {
      ConfigurationManagerInterface& configurationManager = DBus::ConfigurationManager::instance();
      VectorMapStringString toReturn;
      for (int i=0; i < m_pCredentials->rowCount();i++) {
         QModelIndex idx = m_pCredentials->index(i,0);
         MapStringString credentialData;
         QString user = m_pCredentials->data(idx,CredentialModel::Role::NAME).toString();
         QString realm = m_pCredentials->data(idx,CredentialModel::Role::REALM).toString();
         if (user.isEmpty()) {
            user = username();
            m_pCredentials->setData(idx,user,CredentialModel::Role::NAME);
         }
         if (realm.isEmpty()) {
            realm = '*';
            m_pCredentials->setData(idx,realm,CredentialModel::Role::REALM);
         }
         credentialData[ Account::MapField::USERNAME ] = user;
         credentialData[ Account::MapField::PASSWORD ] = m_pCredentials->data(idx,CredentialModel::Role::PASSWORD).toString();
         credentialData[ Account::MapField::REALM    ] = realm;
         toReturn << credentialData;
      }
      configurationManager.setCredentials(id(),toReturn);
   }
}

///Reload all audio codecs
void Account::reloadAudioCodecs()
{
   if (!m_pAudioCodecs) {
      m_pAudioCodecs = new AudioCodecModel(this);
   }
   m_pAudioCodecs->reload();
}

///Save audio codecs
void Account::saveAudioCodecs() {
   if (m_pAudioCodecs)
      m_pAudioCodecs->save();
}

/*****************************************************************************
 *                                                                           *
 *                                 Operator                                  *
 *                                                                           *
 ****************************************************************************/

///Are both account the same
bool Account::operator==(const Account& a)const
{
   return m_AccountId == a.m_AccountId;
}

/*****************************************************************************
 *                                                                           *
 *                                   Video                                   *
 *                                                                           *
 ****************************************************************************/

#undef TO_BOOL
#undef IS_TRUE
