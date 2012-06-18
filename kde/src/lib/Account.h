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
