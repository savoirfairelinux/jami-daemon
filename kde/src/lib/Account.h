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
      
      QString getAccountHostname              () { return getAccountDetail(ACCOUNT_HOSTNAME               )                 ;}
      bool    getAccountEnabled               () { return (getAccountDetail(ACCOUNT_ENABLED               )  == "true")?1:0 ;}
      QString getAccountUsername              () { return getAccountDetail(ACCOUNT_USERNAME               )                 ;}
      QString getAccountMailbox               () { return getAccountDetail(ACCOUNT_MAILBOX                )                 ;}
      bool    getAccountDisplaysAsOnce        () { return (getAccountDetail(ACCOUNT_DISPLAY_SAS_ONCE      )  == "true")?1:0 ;}
      bool    getAccountSrtpRtpFallback       () { return (getAccountDetail(ACCOUNT_SRTP_RTP_FALLBACK     )  == "true")?1:0 ;}
      bool    getAccountZrtpDisplaySas        () { return (getAccountDetail(ACCOUNT_ZRTP_DISPLAY_SAS      )  == "true")?1:0 ;}
      bool    getAccountZrtpNotSuppWarning    () { return (getAccountDetail(ACCOUNT_ZRTP_NOT_SUPP_WARNING )  == "true")?1:0 ;}
      bool    getAccountZrtpHelloHash         () { return (getAccountDetail(ACCOUNT_ZRTP_HELLO_HASH       )  == "true")?1:0 ;}
      bool    getAccountSipStunEnabled        () { return (getAccountDetail(ACCOUNT_SIP_STUN_ENABLED      )  == "true")?1:0 ;}
      QString getAccountSipStunServer         () { return getAccountDetail(ACCOUNT_SIP_STUN_SERVER        )                 ;}
      int     getAccountRegistrationExpire    () { return getAccountDetail(ACCOUNT_REGISTRATION_EXPIRE    ).toInt()         ;}
      bool    getPublishedSameasLocal         () { return (getAccountDetail(PUBLISHED_SAMEAS_LOCAL        )  == "true")?1:0 ;}
      QString getPublishedAddress             () { return getAccountDetail(PUBLISHED_ADDRESS              )                 ;}
      int     getPublishedPort                () { return getAccountDetail(PUBLISHED_PORT                 ).toUInt()        ;}
      QString getTlsPassword                  () { return getAccountDetail(TLS_PASSWORD                   )                 ;}
      int     getTlsListenerPort              () { return getAccountDetail(TLS_LISTENER_PORT              ).toInt()         ;}
      QString getTlsCaListFile                () { return getAccountDetail(TLS_CA_LIST_FILE               )                 ;}
      QString getTlsCertificateFile           () { return getAccountDetail(TLS_CERTIFICATE_FILE           )                 ;}
      QString getTlsPrivateKeyFile            () { return getAccountDetail(TLS_PRIVATE_KEY_FILE           )                 ;}
      QString getTlsCiphers                   () { return getAccountDetail(TLS_CIPHERS                    )                 ;}
      QString getTlsServerName                () { return getAccountDetail(TLS_SERVER_NAME                )                 ;}
      int     getTlsNegotiationTimeoutSec     () { return getAccountDetail(TLS_NEGOTIATION_TIMEOUT_SEC    ).toInt()         ;}
      int     getTlsNegotiationTimeoutMsec    () { return getAccountDetail(TLS_NEGOTIATION_TIMEOUT_MSEC   ).toInt()         ;}
      bool    getTlsVerifyServer              () { return (getAccountDetail(TLS_VERIFY_SERVER             )  == "true")?1:0 ;}
      bool    getTlsVerifyClient              () { return (getAccountDetail(TLS_VERIFY_CLIENT             )  == "true")?1:0 ;}
      bool    getTlsRequireClientCertificate  () { return (getAccountDetail(TLS_REQUIRE_CLIENT_CERTIFICATE)  == "true")?1:0 ;}
      bool    getTlsEnable                    () { return (getAccountDetail(TLS_ENABLE                    )  == "true")?1:0 ;}
      int     getTlsMethod                    () { return getAccountDetail(TLS_METHOD                     ).toInt()         ;}
      QString getAccountAlias                 () { return getAccountDetail(ACCOUNT_ALIAS                  )                 ;}
      bool    getConfigRingToneEnabled        () { return (getAccountDetail(CONFIG_RINGTONE_ENABLED       )  == "true")?1:0 ;}
      QString getConfigRingtonePath           () { return getAccountDetail(CONFIG_RINGTONE_PATH           )                 ;}
      int     getLocalPort                    () { return getAccountDetail(LOCAL_PORT).toInt()                              ;}
      QString getLocalInterface               () { return getAccountDetail(LOCAL_INTERFACE)                                 ;}
      QString getAccountRegistrationStatus    () { return getAccountDetail(ACCOUNT_REGISTRATION_STATUS)                     ;}
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
      void setAccountEnabled                (QString detail){setAccountDetail(ACCOUNT_ENABLED                ,detail);}
      void setAccountRegistrationExpire     (QString detail){setAccountDetail(ACCOUNT_REGISTRATION_EXPIRE    ,detail);}
      void setTlsPassword                   (QString detail){setAccountDetail(TLS_PASSWORD                   ,detail);}
      void setTlsListenerPort               (QString detail){setAccountDetail(TLS_LISTENER_PORT              ,detail);}
      void setTlsCaListFile                 (QString detail){setAccountDetail(TLS_CA_LIST_FILE               ,detail);}
      void setTlsCertificateFile            (QString detail){setAccountDetail(TLS_CERTIFICATE_FILE           ,detail);}
      void setTlsPrivateKeyFile             (QString detail){setAccountDetail(TLS_PRIVATE_KEY_FILE           ,detail);}
      void setTlsMethod                     (QString detail){setAccountDetail(TLS_METHOD                     ,detail);}
      void setTlsCiphers                    (QString detail){setAccountDetail(TLS_CIPHERS                    ,detail);}
      void setTlsServerName                 (QString detail){setAccountDetail(TLS_SERVER_NAME                ,detail);}
      void setTlsNegotiationTimeoutSec      (QString detail){setAccountDetail(TLS_NEGOTIATION_TIMEOUT_SEC    ,detail);}
      void setTlsNegotiationTimeoutMsec     (QString detail){setAccountDetail(TLS_NEGOTIATION_TIMEOUT_MSEC   ,detail);}
      void setAccountSipStunServer          (QString detail){setAccountDetail(ACCOUNT_SIP_STUN_SERVER        ,detail);}
      void setPublishedPort                 (QString detail){setAccountDetail(PUBLISHED_PORT                 ,detail);}
      void setPublishedAddress              (QString detail){setAccountDetail(PUBLISHED_ADDRESS              ,detail);}
      void setLocalPort                     (QString detail){setAccountDetail(LOCAL_PORT                     ,detail);}
      void setLocalInterface                (QString detail){setAccountDetail(LOCAL_INTERFACE                ,detail);}
      void setConfigRingtonePath            (QString detail){setAccountDetail(CONFIG_RINGTONE_PATH           ,detail);}
      void setRingtonePath                  (QString detail){setAccountDetail(CONFIG_RINGTONE_PATH           ,detail);}
      void setTlsVerifyServer               (bool detail   ){setAccountDetail(TLS_VERIFY_SERVER              ,detail?"true":"false");}
      void setTlsVerifyClient               (bool detail   ){setAccountDetail(TLS_VERIFY_CLIENT              ,detail?"true":"false");}
      void setTlsRequireClientCertificate   (bool detail   ){setAccountDetail(TLS_REQUIRE_CLIENT_CERTIFICATE ,detail?"true":"false");}
      void setTlsEnable                     (bool detail   ){setAccountDetail(TLS_ENABLE                     ,detail?"true":"false");}
      void setAccountDisplaySasOnce         (bool detail   ){setAccountDetail(ACCOUNT_DISPLAY_SAS_ONCE       ,detail?"true":"false");}
      void setAccountSrtpRtpFallback        (bool detail   ){setAccountDetail(ACCOUNT_SRTP_RTP_FALLBACK      ,detail?"true":"false");}
      void setAccountZrtpDisplaySas         (bool detail   ){setAccountDetail(ACCOUNT_ZRTP_DISPLAY_SAS       ,detail?"true":"false");}
      void setAccountZrtpNotSuppWarning     (bool detail   ){setAccountDetail(ACCOUNT_ZRTP_NOT_SUPP_WARNING  ,detail?"true":"false");}
      void setAccountZrtpHelloHash          (bool detail   ){setAccountDetail(ACCOUNT_ZRTP_HELLO_HASH        ,detail?"true":"false");}
      void setAccountSipStunEnabled         (bool detail   ){setAccountDetail(ACCOUNT_SIP_STUN_ENABLED       ,detail?"true":"false");}
      void setPublishedSameAsLocal          (bool detail   ){setAccountDetail(PUBLISHED_SAMEAS_LOCAL         ,detail?"true":"false");}
      void setConfigRingtoneEnabled         (bool detail   ){setAccountDetail(CONFIG_RINGTONE_ENABLED        ,detail?"true":"false");}
   
      //Updates
      virtual void updateState();
   
      //Operators
      bool operator==(const Account&)const;
   
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
