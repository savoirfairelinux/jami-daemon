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

#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QtCore/QList>

//Qt
class QString;

//SFLPhone
#include "video/videocodecmodel.h"
#include "keyexchangemodel.h"
#include "tlsmethodmodel.h"
#include "sflphone_const.h"
#include "typedefs.h"
class CredentialModel;
class AudioCodecModel;
class VideoCodecModel;
class RingToneModel  ;
class PhoneNumber    ;
class SecurityValidationModel;
class Certificate    ;

typedef void (Account::*account_function)();

///@enum DtmfType Different method to send the DTMF (key sound) to the peer
enum DtmfType {
   OverRtp,
   OverSip
};
Q_ENUMS(DtmfType)

///Account: a daemon account (SIP or AIX)
class LIB_EXPORT Account : public QObject {
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop

   friend class AccountListModel;

   //Properties
   Q_PROPERTY(QString        alias                        READ alias                         WRITE setAlias                       )
   Q_PROPERTY(Account::Protocol protocol                  READ protocol                      WRITE setProtocol                    )
   Q_PROPERTY(QString        hostname                     READ hostname                      WRITE setHostname                    )
   Q_PROPERTY(QString        username                     READ username                      WRITE setUsername                    )
   Q_PROPERTY(QString        mailbox                      READ mailbox                       WRITE setMailbox                     )
   Q_PROPERTY(QString        proxy                        READ proxy                         WRITE setProxy                       )
   Q_PROPERTY(QString        tlsPassword                  READ tlsPassword                   WRITE setTlsPassword                 )
//    Q_PROPERTY(QString        tlsCaListFile                READ tlsCaListFile                 WRITE setTlsCaListFile               )
//    Q_PROPERTY(QString        tlsCertificateFile           READ tlsCertificateFile            WRITE setTlsCertificateFile          )
//    Q_PROPERTY(QString        tlsPrivateKeyFile            READ tlsPrivateKeyFile             WRITE setTlsPrivateKeyFile           )
   Q_PROPERTY(QString        tlsCiphers                   READ tlsCiphers                    WRITE setTlsCiphers                  )
   Q_PROPERTY(QString        tlsServerName                READ tlsServerName                 WRITE setTlsServerName               )
   Q_PROPERTY(QString        sipStunServer                READ sipStunServer                 WRITE setSipStunServer               )
   Q_PROPERTY(QString        publishedAddress             READ publishedAddress              WRITE setPublishedAddress            )
   Q_PROPERTY(QString        localInterface               READ localInterface                WRITE setLocalInterface              )
   Q_PROPERTY(QString        ringtonePath                 READ ringtonePath                  WRITE setRingtonePath                )
   Q_PROPERTY(QString        lastErrorMessage             READ lastErrorMessage              WRITE setLastErrorMessage            )
   Q_PROPERTY(TlsMethodModel::Type tlsMethod              READ tlsMethod                     WRITE setTlsMethod                   )
   Q_PROPERTY(KeyExchangeModel::Type keyExchange          READ keyExchange                   WRITE setKeyExchange                 )
   Q_PROPERTY(int            lastErrorCode                READ lastErrorCode                 WRITE setLastErrorCode               )
   Q_PROPERTY(int            registrationExpire           READ registrationExpire            WRITE setRegistrationExpire          )
   Q_PROPERTY(int            tlsNegotiationTimeoutSec     READ tlsNegotiationTimeoutSec      WRITE setTlsNegotiationTimeoutSec    )
   Q_PROPERTY(int            tlsNegotiationTimeoutMsec    READ tlsNegotiationTimeoutMsec     WRITE setTlsNegotiationTimeoutMsec   )
   Q_PROPERTY(int            localPort                    READ localPort                     WRITE setLocalPort                   )
   Q_PROPERTY(int            tlsListenerPort              READ tlsListenerPort               WRITE setTlsListenerPort             )
   Q_PROPERTY(int            publishedPort                READ publishedPort                 WRITE setPublishedPort               )
   Q_PROPERTY(bool           enabled                      READ isEnabled                     WRITE setEnabled                     )
   Q_PROPERTY(bool           autoAnswer                   READ isAutoAnswer                  WRITE setAutoAnswer                  )
   Q_PROPERTY(bool           tlsVerifyServer              READ isTlsVerifyServer             WRITE setTlsVerifyServer             )
   Q_PROPERTY(bool           tlsVerifyClient              READ isTlsVerifyClient             WRITE setTlsVerifyClient             )
   Q_PROPERTY(bool           tlsRequireClientCertificate  READ isTlsRequireClientCertificate WRITE setTlsRequireClientCertificate )
   Q_PROPERTY(bool           tlsEnabled                   READ isTlsEnabled                  WRITE setTlsEnabled                  )
   Q_PROPERTY(bool           displaySasOnce               READ isDisplaySasOnce              WRITE setDisplaySasOnce              )
   Q_PROPERTY(bool           srtpRtpFallback              READ isSrtpRtpFallback             WRITE setSrtpRtpFallback             )
   Q_PROPERTY(bool           zrtpDisplaySas               READ isZrtpDisplaySas              WRITE setZrtpDisplaySas              )
   Q_PROPERTY(bool           zrtpNotSuppWarning           READ isZrtpNotSuppWarning          WRITE setZrtpNotSuppWarning          )
   Q_PROPERTY(bool           zrtpHelloHash                READ isZrtpHelloHash               WRITE setZrtpHelloHash               )
   Q_PROPERTY(bool           sipStunEnabled               READ isSipStunEnabled              WRITE setSipStunEnabled              )
   Q_PROPERTY(bool           publishedSameAsLocal         READ isPublishedSameAsLocal        WRITE setPublishedSameAsLocal        )
   Q_PROPERTY(bool           ringtoneEnabled              READ isRingtoneEnabled             WRITE setRingtoneEnabled             )
   Q_PROPERTY(DtmfType       dTMFType                     READ DTMFType                      WRITE setDTMFType                    )
   Q_PROPERTY(int            voiceMailCount               READ voiceMailCount                WRITE setVoiceMailCount              )
//    Q_PROPERTY(QString        typeName                     READ type                          WRITE setType                        )
   Q_PROPERTY(bool           presenceStatus               READ presenceStatus                                                     )
   Q_PROPERTY(QString        presenceMessage              READ presenceMessage                                                    )
   Q_PROPERTY(bool           supportPresencePublish       READ supportPresencePublish                                             )
   Q_PROPERTY(bool           supportPresenceSubscribe     READ supportPresenceSubscribe                                           )
   Q_PROPERTY(bool           presenceEnabled              READ presenceEnabled               WRITE setPresenceEnabled NOTIFY presenceEnabledChanged)
   Q_PROPERTY(bool           videoEnabled                 READ isVideoEnabled                WRITE setVideoEnabled                )
   Q_PROPERTY(int            videoPortMax                 READ videoPortMax                  WRITE setVideoPortMax                )
   Q_PROPERTY(int            videoPortMin                 READ videoPortMin                  WRITE setVideoPortMin                )
   Q_PROPERTY(int            audioPortMax                 READ audioPortMax                  WRITE setAudioPortMax                )
   Q_PROPERTY(int            audioPortMin                 READ audioPortMin                  WRITE setAudioPortMin                )
   Q_PROPERTY(QString        userAgent                    READ userAgent                     WRITE setUserAgent                   )


   public:
      ///@enum AccountEditState: Manage how and when an account can be reloaded or change state
      enum class AccountEditState {
         READY    = 0,
         EDITING  = 1,
         OUTDATED = 2,
         NEW      = 3,
         MODIFIED = 4,
         REMOVED  = 5
      };

      ///@enum AccountEditAction Actions that can be performed on the Account state
      enum class AccountEditAction {
         NOTHING = 0,
         EDIT    = 1,
         RELOAD  = 2,
         SAVE    = 3,
         REMOVE  = 4,
         MODIFY  = 5,
         CANCEL  = 6
      };

      class State {
      public:
         constexpr static const char* REGISTERED                = "REGISTERED"             ;
         constexpr static const char* READY                     = "READY"                  ;
         constexpr static const char* UNREGISTERED              = "UNREGISTERED"           ;
         constexpr static const char* TRYING                    = "TRYING"                 ;
         constexpr static const char* ERROR                     = "ERROR"                  ;
         constexpr static const char* ERROR_AUTH                = "ERRORAUTH"              ;
         constexpr static const char* ERROR_NETWORK             = "ERRORNETWORK"           ;
         constexpr static const char* ERROR_HOST                = "ERRORHOST"              ;
         constexpr static const char* ERROR_CONF_STUN           = "ERROR_CONF_STUN"        ;
         constexpr static const char* ERROR_EXIST_STUN          = "ERROREXISTSTUN"         ;
         constexpr static const char* ERROR_SERVICE_UNAVAILABLE = "ERRORSERVICEUNAVAILABLE";
         constexpr static const char* ERROR_NOT_ACCEPTABLE      = "ERRORNOTACCEPTABLE"     ;
         constexpr static const char* REQUEST_TIMEOUT           = "Request Timeout"        ;
      };

      class RegistrationEnabled {
      public:
         constexpr static const char* YES  = "true";
         constexpr static const char* NO   = "false";
      };

      ~Account();
      //Constructors
      static Account* buildExistingAccountFromId(const QString& _accountId);
      static Account* buildNewAccountFromAlias  (const QString& alias     );

      enum Role {
         Alias                       = 100,
         Proto                       = 101,
         Hostname                    = 102,
         Username                    = 103,
         Mailbox                     = 104,
         Proxy                       = 105,
         TlsPassword                 = 107,
         TlsCaListCertificate        = 108,
         TlsCertificate              = 109,
         TlsPrivateKeyCertificate    = 110,
         TlsCiphers                  = 111,
         TlsServerName               = 112,
         SipStunServer               = 113,
         PublishedAddress            = 114,
         LocalInterface              = 115,
         RingtonePath                = 116,
         TlsMethod                   = 117,
         KeyExchange                 = 190,
         RegistrationExpire          = 118,
         TlsNegotiationTimeoutSec    = 119,
         TlsNegotiationTimeoutMsec   = 120,
         LocalPort                   = 121,
         TlsListenerPort             = 122,
         PublishedPort               = 123,
         Enabled                     = 124,
         AutoAnswer                  = 125,
         TlsVerifyServer             = 126,
         TlsVerifyClient             = 127,
         TlsRequireClientCertificate = 128,
         TlsEnabled                  = 129,
         DisplaySasOnce              = 130,
         SrtpRtpFallback             = 131,
         ZrtpDisplaySas              = 132,
         ZrtpNotSuppWarning          = 133,
         ZrtpHelloHash               = 134,
         SipStunEnabled              = 135,
         PublishedSameAsLocal        = 136,
         RingtoneEnabled             = 137,
         dTMFType                    = 138,
         Id                          = 139,
         Object                      = 140,
         TypeName                    = 141,
         PresenceStatus              = 142,
         PresenceMessage             = 143,
      };

      class MapField {
      public:
         constexpr static const char* ID                     = "Account.id"                        ;
         constexpr static const char* TYPE                   = "Account.type"                      ;
         constexpr static const char* ALIAS                  = "Account.alias"                     ;
         constexpr static const char* ENABLED                = "Account.enable"                    ;
         constexpr static const char* MAILBOX                = "Account.mailbox"                   ;
         constexpr static const char* DTMF_TYPE              = "Account.dtmfType"                  ;
         constexpr static const char* AUTOANSWER             = "Account.autoAnswer"                ;
         constexpr static const char* HOSTNAME               = "Account.hostname"                  ;
         constexpr static const char* USERNAME               = "Account.username"                  ;
         constexpr static const char* ROUTE                  = "Account.routeset"                  ;
         constexpr static const char* PASSWORD               = "Account.password"                  ;
         constexpr static const char* REALM                  = "Account.realm"                     ;
         constexpr static const char* LOCAL_INTERFACE        = "Account.localInterface"            ;
         constexpr static const char* PUBLISHED_SAMEAS_LOCAL = "Account.publishedSameAsLocal"      ;
         constexpr static const char* LOCAL_PORT             = "Account.localPort"                 ;
         constexpr static const char* PUBLISHED_PORT         = "Account.publishedPort"             ;
         constexpr static const char* PUBLISHED_ADDRESS      = "Account.publishedAddress"          ;
         constexpr static const char* USER_AGENT             = "Account.useragent"                 ;
         class Audio {
         public:
            constexpr static const char* PORT_MAX            = "Account.audioPortMax"              ;
            constexpr static const char* PORT_MIN            = "Account.audioPortMin"              ;
         };
         class Video {
         public:
            constexpr static const char* ENABLED             = "Account.videoEnabled"              ;
            constexpr static const char* PORT_MAX            = "Account.videoPortMax"              ;
            constexpr static const char* PORT_MIN            = "Account.videoPortMin"              ;
         };
         class STUN {
         public:
            constexpr static const char* SERVER              = "STUN.server"                       ;
            constexpr static const char* ENABLED             = "STUN.enable"                       ;
         };
         class Presence {
         public:
            constexpr static const char* SUPPORT_PUBLISH     = "Account.presencePublishSupported"  ;
            constexpr static const char* SUPPORT_SUBSCRIBE   = "Account.presenceSubscribeSupported";
            constexpr static const char* ENABLED             = "Account.presenceEnabled"           ;
         };
         class Registration {
         public:
            constexpr static const char* EXPIRE              = "Account.registrationExpire"        ;
            constexpr static const char* STATUS              = "Account.registrationStatus"        ;
         };
         class Ringtone {
         public:
            constexpr static const char* PATH                = "Account.ringtonePath"              ;
            constexpr static const char* ENABLED             = "Account.ringtoneEnabled"           ;
         };
         class SRTP {
         public:
            constexpr static const char* KEY_EXCHANGE        = "SRTP.keyExchange"                  ;
            constexpr static const char* ENABLED             = "SRTP.enable"                       ;
            constexpr static const char* RTP_FALLBACK        = "SRTP.rtpFallback"                  ;
         };
         class ZRTP {
         public:
            constexpr static const char* DISPLAY_SAS         = "ZRTP.displaySAS"                   ;
            constexpr static const char* NOT_SUPP_WARNING    = "ZRTP.notSuppWarning"               ;
            constexpr static const char* HELLO_HASH          = "ZRTP.helloHashEnable"              ;
            constexpr static const char* DISPLAY_SAS_ONCE    = "ZRTP.displaySasOnce"               ;
         };
         class TLS {
         public:
            constexpr static const char* LISTENER_PORT       = "TLS.listenerPort"                  ;
            constexpr static const char* ENABLED             = "TLS.enable"                        ;
            constexpr static const char* PORT                = "TLS.port"                          ;
            constexpr static const char* CA_LIST_FILE        = "TLS.certificateListFile"           ;
            constexpr static const char* CERTIFICATE_FILE    = "TLS.certificateFile"               ;
            constexpr static const char* PRIVATE_KEY_FILE    = "TLS.privateKeyFile"                ;
            constexpr static const char* PASSWORD            = "TLS.password"                      ;
            constexpr static const char* METHOD              = "TLS.method"                        ;
            constexpr static const char* CIPHERS             = "TLS.ciphers"                       ;
            constexpr static const char* SERVER_NAME         = "TLS.serverName"                    ;
            constexpr static const char* VERIFY_SERVER       = "TLS.verifyServer"                  ;
            constexpr static const char* VERIFY_CLIENT       = "TLS.verifyClient"                  ;
            constexpr static const char* REQUIRE_CLIENT_CERTIFICATE = "TLS.requireClientCertificate";
            constexpr static const char* NEGOTIATION_TIMEOUT_SEC    = "TLS.negotiationTimeoutSec"   ;
            constexpr static const char* NEGOTIATION_TIMEOUT_MSEC   = "TLS.negotiationTimemoutMsec" ;
         };
      };

      class ProtocolName {
      public:
         constexpr static const char* SIP   = "SIP"  ;
         constexpr static const char* IAX   = "IAX"  ;
         constexpr static const char* IP2IP = "IP2IP";
      };

      enum class Protocol {
         SIP = 0,
         IAX = 1,
      };
      Q_ENUMS(Protocol)

      /**
       *Perform an action
       * @return If the state changed
       */
      bool performAction(Account::AccountEditAction action);
      Account::AccountEditState state() const;

      //Getters
      bool            isNew()                             const;
      const QString   id()                                const;
      const QString   toHumanStateName()                  const;
      const QString   accountDetail(const QString& param) const;
      const QString   alias()                             const;
      bool            isRegistered()                      const;
      QModelIndex     index()                                  ;
      QString         stateColorName()                    const;
      QVariant        stateColor()                        const;

      Q_INVOKABLE CredentialModel*  credentialsModel() const;
      Q_INVOKABLE AudioCodecModel*  audioCodecModel () const;
      Q_INVOKABLE VideoCodecModel*  videoCodecModel () const;
      Q_INVOKABLE RingToneModel*    ringToneModel   () const;
      Q_INVOKABLE KeyExchangeModel* keyExchangeModel() const;
      Q_INVOKABLE SecurityValidationModel* securityValidationModel() const;

      //Getters
      QString hostname                     () const;
      bool    isEnabled                    () const;
      bool    isAutoAnswer                 () const;
      QString username                     () const;
      QString mailbox                      () const;
      QString proxy                        () const;
      QString password                     () const;
      bool    isDisplaySasOnce             () const;
      bool    isSrtpRtpFallback            () const;
      bool    isSrtpEnabled                () const;
      bool    isZrtpDisplaySas             () const;
      bool    isZrtpNotSuppWarning         () const;
      bool    isZrtpHelloHash              () const;
      bool    isSipStunEnabled             () const;
      QString sipStunServer                () const;
      int     registrationExpire           () const;
      bool    isPublishedSameAsLocal       () const;
      QString publishedAddress             () const;
      int     publishedPort                () const;
      QString tlsPassword                  () const;
      int     tlsListenerPort              () const;
      Certificate* tlsCaListCertificate    () const;
      Certificate* tlsCertificate          () const;
      Certificate* tlsPrivateKeyCertificate() const;
      QString tlsCiphers                   () const;
      QString tlsServerName                () const;
      int     tlsNegotiationTimeoutSec     () const;
      int     tlsNegotiationTimeoutMsec    () const;
      bool    isTlsVerifyServer            () const;
      bool    isTlsVerifyClient            () const;
      bool    isTlsRequireClientCertificate() const;
      bool    isTlsEnabled                 () const;
      bool    isRingtoneEnabled            () const;
      QString ringtonePath                 () const;
      QString lastErrorMessage             () const;
      int     lastErrorCode                () const;
      int     localPort                    () const;
      int     voiceMailCount               () const;
      QString localInterface               () const;
      QString registrationStatus           () const;
      DtmfType DTMFType                    () const;
      bool    presenceStatus               () const;
      QString presenceMessage              () const;
      bool    supportPresencePublish       () const;
      bool    supportPresenceSubscribe     () const;
      bool    presenceEnabled              () const;
      bool    isVideoEnabled               () const;
      int     videoPortMax                 () const;
      int     videoPortMin                 () const;
      int     audioPortMin                 () const;
      int     audioPortMax                 () const;
      QString userAgent                    () const;
      Account::Protocol      protocol      () const;
      TlsMethodModel::Type   tlsMethod     () const;
      KeyExchangeModel::Type keyExchange   () const;
      QVariant roleData            (int role) const;

      //Setters
      void setId      (const QString& id);
      void setAlias                         (const QString& detail);
      void setProtocol                      (Account::Protocol proto);
      void setHostname                      (const QString& detail );
      void setUsername                      (const QString& detail );
      void setMailbox                       (const QString& detail );
      void setProxy                         (const QString& detail );
      void setPassword                      (const QString& detail );
      void setTlsPassword                   (const QString& detail );
      void setTlsCaListCertificate          (Certificate* cert     );
      void setTlsCertificate                (Certificate* cert     );
      void setTlsPrivateKeyCertificate      (Certificate* cert     );
      void setTlsCiphers                    (const QString& detail );
      void setTlsServerName                 (const QString& detail );
      void setSipStunServer                 (const QString& detail );
      void setPublishedAddress              (const QString& detail );
      void setLocalInterface                (const QString& detail );
      void setRingtonePath                  (const QString& detail );
      void setLastErrorMessage              (const QString& message);
      void setTlsMethod                     (TlsMethodModel::Type   detail);
      void setKeyExchange                   (KeyExchangeModel::Type detail);
      void setLastErrorCode                 (int  code  );
      void setVoiceMailCount                (int  count );
      void setRegistrationExpire            (int  detail);
      void setTlsNegotiationTimeoutSec      (int  detail);
      void setTlsNegotiationTimeoutMsec     (int  detail);
      void setLocalPort                     (unsigned short detail);
      void setTlsListenerPort               (unsigned short detail);
      void setPublishedPort                 (unsigned short detail);
      void setAutoAnswer                    (bool detail);
      void setTlsVerifyServer               (bool detail);
      void setTlsVerifyClient               (bool detail);
      void setTlsRequireClientCertificate   (bool detail);
      void setTlsEnabled                    (bool detail);
      void setDisplaySasOnce                (bool detail);
      void setSrtpRtpFallback               (bool detail);
      void setSrtpEnabled                   (bool detail);
      void setZrtpDisplaySas                (bool detail);
      void setZrtpNotSuppWarning            (bool detail);
      void setZrtpHelloHash                 (bool detail);
      void setSipStunEnabled                (bool detail);
      void setPublishedSameAsLocal          (bool detail);
      void setRingtoneEnabled               (bool detail);
      void setPresenceEnabled               (bool enable);
      void setVideoEnabled                  (bool enable);
      void setAudioPortMax                  (int port   );
      void setAudioPortMin                  (int port   );
      void setVideoPortMax                  (int port   );
      void setVideoPortMin                  (int port   );
      void setDTMFType                      (DtmfType type);
      void setUserAgent                     (const QString& agent);

      void setRoleData(int role, const QVariant& value);

      //Updates
      virtual bool updateState();

      //Operators
      bool operator==(const Account&)const;

      //Mutator
      Q_INVOKABLE void saveCredentials  ();
      Q_INVOKABLE void saveAudioCodecs  ();
      Q_INVOKABLE void reloadCredentials();
      Q_INVOKABLE void reloadAudioCodecs();


   public Q_SLOTS:
      void setEnabled(bool checked);

   private Q_SLOTS:
      void slotPresentChanged        (bool  present  );
      void slotPresenceMessageChanged(const QString& );
      void slotUpdateCertificate     (               );

   private:
      //Constructors
      Account();

      //Attributes
      QString                 m_AccountId      ;
      QHash<QString,QString>  m_hAccountDetails;
      PhoneNumber*            m_pAccountNumber ;

      //Setters
      void setAccountDetails (const QHash<QString,QString>& m          );
      bool setAccountDetail  (const QString& param, const QString& val );

      //State actions
      void nothing() {};
      void edit()    {m_CurrentState = AccountEditState::EDITING ;emit changed(this);};
      void modify()  {m_CurrentState = AccountEditState::MODIFIED;emit changed(this);};
      void remove()  {m_CurrentState = AccountEditState::REMOVED ;emit changed(this);};
      void cancel()  {m_CurrentState = AccountEditState::READY   ;emit changed(this);};
      void outdate() {m_CurrentState = AccountEditState::OUTDATED;emit changed(this);};
      void reload();
      void save();
      void reloadMod() {reload();modify();};

      CredentialModel*  m_pCredentials     ;
      AudioCodecModel*  m_pAudioCodecs     ;
      VideoCodecModel*  m_pVideoCodecs     ;
      RingToneModel*    m_pRingToneModel   ;
      KeyExchangeModel* m_pKeyExchangeModel;
      SecurityValidationModel* m_pSecurityValidationModel;
      AccountEditState m_CurrentState;
      static const account_function stateMachineActionsOnState[6][7];

      //Cached account details (as they are called too often for the hash)
      QString m_HostName;
      QString m_LastErrorMessage;
      int     m_LastErrorCode;
      int     m_VoiceMailCount;
      Certificate* m_pCaCert;
      Certificate* m_pTlsCert;
      Certificate* m_pPrivateKey;


   Q_SIGNALS:
      ///The account state (Invalid,Trying,Registered) changed
      void stateChanged(QString state);
      void detailChanged(Account* a,QString name,QString newVal, QString oldVal);
      void changed(Account* a);
      ///The alias changed, take effect instantaneously
      void aliasChanged(const QString&);
      ///The presence support changed
      void presenceEnabledChanged(bool);
};
// Q_DISABLE_COPY(Account)
Q_DECLARE_METATYPE(Account*)
#endif
