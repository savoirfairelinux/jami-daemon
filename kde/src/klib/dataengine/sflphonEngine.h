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

#ifndef SFLPHONEENGINE_H
#define SFLPHONEENGINE_H

#include <Plasma/DataEngine>
#include <Plasma/Service>
#include <QHash>

#include "../../lib/CallModel.h"
#include "../SortableDockCommon.h"

typedef QHash<QString,QVariant> HashStringString;
typedef QHash<QString,QHash<QString,QVariant> > ContactHash;
class Call;

class SFLPhoneEngine : public Plasma::DataEngine,public SortableDockCommon<>
{
   Q_OBJECT

   public:
      SFLPhoneEngine(QObject* parent, const QVariantList& args);
      Plasma::Service *serviceForSource(const QString &source);
      virtual QStringList sources() const;

      static CallModel<>* getModel();
      
      friend class SFLPhoneService;

   protected:
      bool sourceRequestEvent(const QString& name);
      bool updateSourceEvent(const QString& source);

   private:
      QHash<QString, HashStringString > historyCall        ;
      QHash<QString, HashStringString > currentCall        ;
      QHash<QString, QStringList>       currentConferences ;
      static CallModel<>*               m_pModel           ;
      ContactHash                       m_hContacts        ;
      QString getCallStateName(call_state state);
      void updateHistory        ();
      void updateCallList       ();
      void updateAccounts       ();
      void updateConferenceList ();
      void updateContacts       ();
      void updateBookmarkList   ();
      void updateInfo();
      
      void generateNumberList(QString name);
   private slots:
      void updateCollection();
      void callStateChangedSignal  (Call* call);
      void incomingCallSignal      (Call* conf);
      void conferenceCreatedSignal (Call* conf);
      void conferenceChangedSignal (Call* conf);
      //void conferenceRemovedSignal(const QString& confId);
      void incomingMessageSignal( const QString& accountId, const QString& message );
      void voiceMailNotifySignal( const QString& accountId, int count              );
      void accountChanged();
};

#endif // SFLPHONEENGINE_H
