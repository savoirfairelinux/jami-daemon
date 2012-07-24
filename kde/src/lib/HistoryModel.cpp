/************************************************************************************
 *   Copyright (C) 2012 by Savoir-Faire Linux                                       *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
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
#include "HistoryModel.h"
#include "callmanager_interface_singleton.h"
#include "configurationmanager_interface_singleton.h"
#include "Call.h"


/*****************************************************************************
 *                                                                           *
 *                             Private classes                               *
 *                                                                           *
 ****************************************************************************/

///SortableCallSource: helper class to make sorting possible
class SortableCallSource {
public:
   SortableCallSource(Call* call=0) : count(0),callInfo(call) {}
   uint count;
   Call* callInfo;
   bool operator<(SortableCallSource other) {
      return (other.count > count);
   }
};

inline bool operator< (const SortableCallSource & s1, const SortableCallSource & s2)
{
    return  s1.count < s2.count;
}

HistoryModel* HistoryModel::m_spInstance    = nullptr;
CallMap       HistoryModel::m_sHistoryCalls          ;


/*****************************************************************************
 *                                                                           *
 *                                 Constructor                               *
 *                                                                           *
 ****************************************************************************/

///Constructor
HistoryModel::HistoryModel():m_HistoryInit(false)
{
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   const QVector< QMap<QString, QString> > history = configurationManager.getHistory();
   foreach (const MapStringString& hc, history) {
      Call* pastCall = Call::buildHistoryCall(
               hc[ CALLID_KEY          ]         ,
               hc[ TIMESTAMP_START_KEY ].toUInt(),
               hc[ TIMESTAMP_STOP_KEY  ].toUInt(),
               hc[ ACCOUNT_ID_KEY      ]         ,
               hc[ DISPLAY_NAME_KEY    ]         ,
               hc[ PEER_NUMBER_KEY     ]         ,
               hc[ STATE_KEY           ]
      );
      if (pastCall->getPeerName().isEmpty()) {
         pastCall->setPeerName("Unknown");
      }
      pastCall->setRecordingPath(hc[ RECORDING_PATH_KEY ]);
      addPriv(pastCall);
   }
   m_HistoryInit = true;
} //initHistory

///Destructor
HistoryModel::~HistoryModel()
{
   m_spInstance = nullptr;
}

///Singleton
HistoryModel* HistoryModel::self()
{
   if (!m_spInstance)
      m_spInstance = new HistoryModel();
   return m_spInstance;
}


/*****************************************************************************
 *                                                                           *
 *                           History related code                            *
 *                                                                           *
 ****************************************************************************/

///Add to history
void HistoryModel::add(Call* call)
{
   self()->addPriv(call);
}

///Add to history
void HistoryModel::addPriv(Call* call)
{
   if (call) {
      m_sHistoryCalls[call->getStartTimeStamp()] = call;
   }
   emit newHistoryCall(call);
   emit historyChanged();
}

///Return the history list
const CallMap& HistoryModel::getHistory()
{
   self();
   return m_sHistoryCalls;
}

///Return a list of all previous calls
const QStringList HistoryModel::getHistoryCallId()
{
   self();
   QStringList toReturn;
   foreach(Call* call, m_sHistoryCalls) {
      toReturn << call->getCallId();
   }
   return toReturn;
}

///Sort all history call by popularity and return the result (most popular first)
const QStringList HistoryModel::getNumbersByPopularity()
{
   self();
   QHash<QString,SortableCallSource*> hc;
   foreach (Call* call, getHistory()) {
      if (!hc[call->getPeerPhoneNumber()]) {
         hc[call->getPeerPhoneNumber()] = new SortableCallSource(call);
      }
      hc[call->getPeerPhoneNumber()]->count++;
   }
   QList<SortableCallSource> userList;
   foreach (SortableCallSource* i,hc) {
      userList << *i;
   }
   qSort(userList);
   QStringList cl;
   for (int i=userList.size()-1;i >=0 ;i--) {
      cl << userList[i].callInfo->getPeerPhoneNumber();
   }
   foreach (SortableCallSource* i,hc) {
      delete i;
   }

   return cl;
} //getNumbersByPopularity
