/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
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

#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QtCore/QList>

//Qt
class QString;

//SFLPhone
#include "VideoCodec.h"
#include "sflphone_const.h"
#include "typedefs.h"
#include "dbus/metatypes.h"

const QString& account_state_name(const QString& s);

///Account: a daemon account (SIP or AIX)
class LIB_EXPORT Account : public QObject {
   Q_OBJECT

   public:
      ~Account();
      //Constructors
      static Account* buildExistingAccountFromId(const QString& _accountId);
      static Account* buildNewAccountFromAlias(const QString& alias);
   
      //Getters
      bool                    isNew()                                const;
      const QString&          getAccountId()                         const;
      const MapStringString&  getAccountDetails()                    const;
      const QString&          getStateName(const QString& state)     const;
      const QString&          getAccountDetail(const QString& param) const;
      const QString&          getAlias()                             const;
      bool                    isEnabled()                            const;
      bool                    isRegistered()                         const;

      ///Return the account hostname
      QString getAccountHostname              () { return getAccountDetail(ACCOUNT_HOSTNAME               )                 ;}
      ///Return if the account is enabled
      bool    isAccountEnabled                () { return (getAccountDetail(ACCOUNT_ENABLED               )  == "true")?1:0 ;}
      ///Return the account user name
      QString getAccountUsername              () { return getAccountDetail(ACCOUNT_USERNAME               )                 ;}
      ///Return the account mailbox address
      QString getAccountMailbox               () { return getAccountDetail(ACCOUNT_MAILBOX                )                 ;}
      ///
      bool    isAccountDisplaysAsOnce         () { return (getAccountDetail(ACCOUNT_DISPLAY_SAS_ONCE      )  == "true")?1:0 ;}
      ///Return the account security fallback
      bool    getAccountSrtpRtpFallback       () { return (getAccountDetail(ACCOUNT_SRTP_RTP_FALLBACK     )  == "true")?1:0 ;}
      ///
      bool    isAccountZrtpDisplaySas         () { return (getAccountDetail(ACCOUNT_ZRTP_DISPLAY_SAS      )  == "true")?1:0 ;}
      ///Return if the other side support warning
      bool    isAccountZrtpNotSuppWarning     () { return (getAccountDetail(ACCOUNT_ZRTP_NOT_SUPP_WARNING )  == "true")?1:0 ;}
      ///
      bool    isAccountZrtpHelloHash          () { return (getAccountDetail(ACCOUNT_ZRTP_HELLO_HASH       )  == "true")?1:0 ;}
      ///Return if the account is using a STUN server
      bool    isAccountSipStunEnabled         () { return (getAccountDetail(ACCOUNT_SIP_STUN_ENABLED      )  == "true")?1:0 ;}
      ///Return the account STUN server
      QString getAccountSipStunServer         () { return getAccountDetail(ACCOUNT_SIP_STUN_SERVER        )                 ;}
      ///Return when the account expire (require renewal)
      int     getAccountRegistrationExpire    () { return getAccountDetail(ACCOUNT_REGISTRATION_EXPIRE    ).toInt()         ;}
      ///Return if the published address is the same as the local one
      bool    isPublishedSameasLocal          () { return (getAccountDetail(PUBLISHED_SAMEAS_LOCAL        )  == "true")?1:0 ;}
      ///Return the account published address
      QString getPublishedAddress             () { return getAccountDetail(PUBLISHED_ADDRESS              )                 ;}
      ///Return the account published port
      int     getPublishedPort                () { return getAccountDetail(PUBLISHED_PORT                 ).toUInt()        ;}
      ///Return the account tls password
      QString getTlsPassword                  () { return getAccountDetail(TLS_PASSWORD                   )                 ;}
      ///Return the account TLS port
      int     getTlsListenerPort              () { return getAccountDetail(TLS_LISTENER_PORT              ).toInt()         ;}
      ///Return the account TLS certificate authority list file
      QString getTlsCaListFile                () { return getAccountDetail(TLS_CA_LIST_FILE               )                 ;}
      ///Return the account TLS certificate
      QString getTlsCertificateFile           () { return getAccountDetail(TLS_CERTIFICATE_FILE           )                 ;}
      ///Return the account private key
      QString getTlsPrivateKeyFile            () { return getAccountDetail(TLS_PRIVATE_KEY_FILE           )                 ;}
      ///Return the account cipher
      QString getTlsCiphers                   () { return getAccountDetail(TLS_CIPHERS                    )                 ;}
      ///Return the account TLS server name
      QString getTlsServerName                () { return getAccountDetail(TLS_SERVER_NAME                )                 ;}
      ///Return the account negotiation timeout in seconds
      int     getTlsNegotiationTimeoutSec     () { return getAccountDetail(TLS_NEGOTIATION_TIMEOUT_SEC    ).toInt()         ;}
      ///Return the account negotiation timeout in milliseconds
      int     getTlsNegotiationTimeoutMsec    () { return getAccountDetail(TLS_NEGOTIATION_TIMEOUT_MSEC   ).toInt()         ;}
      ///Return the account TLS verify server
      bool    isTlsVerifyServer               () { return (getAccountDetail(TLS_VERIFY_SERVER             )  == "true")?1:0 ;}
      ///Return the account TLS verify client
      bool    isTlsVerifyClient               () { return (getAccountDetail(TLS_VERIFY_CLIENT             )  == "true")?1:0 ;}
      ///Return if it is required for the peer to have a certificate
      bool    isTlsRequireClientCertificate  () { return (getAccountDetail(TLS_REQUIRE_CLIENT_CERTIFICATE)  == "true")?1:0 ;}
      ///Return the account TLS security is enabled
      bool    isTlsEnable                     () { return (getAccountDetail(TLS_ENABLE                    )  == "true")?1:0 ;}
      ///Return the account the TLS encryption method
      int     getTlsMethod                    () { return getAccountDetail(TLS_METHOD                     ).toInt()         ;}
      ///Return the account alias
      QString getAccountAlias                 () { return getAccountDetail(ACCOUNT_ALIAS                  )                 ;}
      ///Return if the ringtone are enabled
      bool    isConfigRingToneEnabled         () { return (getAccountDetail(CONFIG_RINGTONE_ENABLED       )  == "true")?1:0 ;}
      ///Return the account ringtone path
      QString getConfigRingtonePath           () { return getAccountDetail(CONFIG_RINGTONE_PATH           )                 ;}
      ///Return the account local port
      int     getLocalPort                    () { return getAccountDetail(LOCAL_PORT).toInt()                              ;}
      ///Return the account local interface
      QString getLocalInterface               () { return getAccountDetail(LOCAL_INTERFACE)                                 ;}
      ///Return the account registration status
      QString getAccountRegistrationStatus    () { return getAccountDetail(ACCOUNT_REGISTRATION_STATUS)                     ;}
      ///Return the account type
      QString getAccountType                  () { return getAccountDetail(ACCOUNT_TYPE)                                    ;}
   
      //Setters
      void setAccountId      (const QString& id                        );
      void setAccountDetails (const MapStringString& m                 );
      void setAccountDetail  (const QString& param, const QString& val );
      #ifdef ENABLE_VIDEO
      void setActiveVideoCodecList(QList<VideoCodec*> codecs);
      QList<VideoCodec*> getActiveVideoCodecList();
      #endif
      void setAccountAlias                  (QString detail){setAccountDetail(ACCOUNT_ALIAS                  ,detail);}
      void setAccountType                   (QString detail){setAccountDetail(ACCOUNT_TYPE                   ,detail);}
      void setAccountHostname               (QString detail){setAccountDetail(ACCOUNT_HOSTNAME               ,detail);}
      void setAccountUsername               (QString detail){setAccountDetail(ACCOUNT_USERNAME               ,detail);}
      void setAccountPassword               (QString detail){setAccountDetail(ACCOUNT_PASSWORD               ,detail);}
      void setAccountMailbox                (QString detail){setAccountDetail(ACCOUNT_MAILBOX                ,detail);}
      void setTlsPassword                   (QString detail){setAccountDetail(TLS_PASSWORD                   ,detail);}
      void setTlsCaListFile                 (QString detail){setAccountDetail(TLS_CA_LIST_FILE               ,detail);}
      void setTlsCertificateFile            (QString detail){setAccountDetail(TLS_CERTIFICATE_FILE           ,detail);}
      void setTlsPrivateKeyFile             (QString detail){setAccountDetail(TLS_PRIVATE_KEY_FILE           ,detail);}
      void setTlsCiphers                    (QString detail){setAccountDetail(TLS_CIPHERS                    ,detail);}
      void setTlsServerName                 (QString detail){setAccountDetail(TLS_SERVER_NAME                ,detail);}
      void setAccountSipStunServer          (QString detail){setAccountDetail(ACCOUNT_SIP_STUN_SERVER        ,detail);}
      void setPublishedAddress              (QString detail){setAccountDetail(PUBLISHED_ADDRESS              ,detail);}
      void setLocalInterface                (QString detail){setAccountDetail(LOCAL_INTERFACE                ,detail);}
      void setConfigRingtonePath            (QString detail){setAccountDetail(CONFIG_RINGTONE_PATH           ,detail);}
      void setRingtonePath                  (QString detail){setAccountDetail(CONFIG_RINGTONE_PATH           ,detail);}
      void setTlsMethod                     (int     detail){setAccountDetail(TLS_METHOD                     ,QString::number(detail));}
      void setAccountRegistrationExpire     (int     detail){setAccountDetail(ACCOUNT_REGISTRATION_EXPIRE    ,QString::number(detail));}
      void setTlsNegotiationTimeoutSec      (int     detail){setAccountDetail(TLS_NEGOTIATION_TIMEOUT_SEC    ,QString::number(detail));}
      void setTlsNegotiationTimeoutMsec     (int     detail){setAccountDetail(TLS_NEGOTIATION_TIMEOUT_MSEC   ,QString::number(detail));}
      void setLocalPort                     (short   detail){setAccountDetail(LOCAL_PORT                     ,QString::number(detail));}
      void setTlsListenerPort               (short   detail){setAccountDetail(TLS_LISTENER_PORT              ,QString::number(detail));}
      void setPublishedPort                 (short   detail){setAccountDetail(PUBLISHED_PORT                 ,QString::number(detail));}
      void setAccountEnabled                (bool    detail){setAccountDetail(ACCOUNT_ENABLED                ,detail?"true":"false");}
      void setTlsVerifyServer               (bool    detail){setAccountDetail(TLS_VERIFY_SERVER              ,detail?"true":"false");}
      void setTlsVerifyClient               (bool    detail){setAccountDetail(TLS_VERIFY_CLIENT              ,detail?"true":"false");}
      void setTlsRequireClientCertificate   (bool    detail){setAccountDetail(TLS_REQUIRE_CLIENT_CERTIFICATE ,detail?"true":"false");}
      void setTlsEnable                     (bool    detail){setAccountDetail(TLS_ENABLE                     ,detail?"true":"false");}
      void setAccountDisplaySasOnce         (bool    detail){setAccountDetail(ACCOUNT_DISPLAY_SAS_ONCE       ,detail?"true":"false");}
      void setAccountSrtpRtpFallback        (bool    detail){setAccountDetail(ACCOUNT_SRTP_RTP_FALLBACK      ,detail?"true":"false");}
      void setAccountZrtpDisplaySas         (bool    detail){setAccountDetail(ACCOUNT_ZRTP_DISPLAY_SAS       ,detail?"true":"false");}
      void setAccountZrtpNotSuppWarning     (bool    detail){setAccountDetail(ACCOUNT_ZRTP_NOT_SUPP_WARNING  ,detail?"true":"false");}
      void setAccountZrtpHelloHash          (bool    detail){setAccountDetail(ACCOUNT_ZRTP_HELLO_HASH        ,detail?"true":"false");}
      void setAccountSipStunEnabled         (bool    detail){setAccountDetail(ACCOUNT_SIP_STUN_ENABLED       ,detail?"true":"false");}
      void setPublishedSameAsLocal          (bool    detail){setAccountDetail(PUBLISHED_SAMEAS_LOCAL         ,detail?"true":"false");}
      void setConfigRingtoneEnabled         (bool    detail){setAccountDetail(CONFIG_RINGTONE_ENABLED        ,detail?"true":"false");}
   
      //Updates
      virtual void updateState();
   
      //Operators
      bool operator==(const Account&)const;

      //Mutator
      void save();
   
   protected:
      //Constructors
      Account();

      //Attributes
      QString*         m_pAccountId;
      MapStringString* m_pAccountDetails;

   public slots:
      void setEnabled(bool checked);

   private slots:
      void accountChanged(QString accountId,QString stateName, int state);

   signals:
      ///The account state (Invalif,Trying,Registered) changed
      void stateChanged(QString state);
};
#endif
