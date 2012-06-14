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
