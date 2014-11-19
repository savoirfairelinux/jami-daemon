/************************************************************************************
 *   Copyright (C) 2014 by Savoir-Faire Linux                                       *
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
#include "legacyhistorybackend.h"

//SFLPhone
#include "dbus/configurationmanager.h"
#include "call.h"

LegacyHistoryBackend::LegacyHistoryBackend(QObject* parent) : AbstractHistoryBackend(nullptr,parent)
{
   setObjectName("LegacyHistoryBackend");
}

LegacyHistoryBackend::~LegacyHistoryBackend()
{

}


QString LegacyHistoryBackend::name () const
{
   return "Legacy history backend";
}

QVariant LegacyHistoryBackend::icon() const
{
   return QVariant();
}

bool LegacyHistoryBackend::isEnabled() const
{
   return false; //This one is never considered enabled
}

bool LegacyHistoryBackend::load()
{
   ConfigurationManagerInterface& configurationManager = DBus::ConfigurationManager::instance();
   const QVector< QMap<QString, QString> > history = configurationManager.getHistory();
   for(int i = history.size()-1;i>=0;i--) {
      const MapStringString& hc = history[i];
      Call* pastCall = Call::buildHistoryCall(hc);
      if (pastCall->peerName().isEmpty()) {
         pastCall->setPeerName(tr("Unknown"));
      }
      pastCall->setRecordingPath(hc[ Call::HistoryMapFields::RECORDING_PATH ]);
      emit newHistoryCallAdded(pastCall);
   }
   return true;
}

bool LegacyHistoryBackend::reload()
{
   return false;
}

bool LegacyHistoryBackend::append(const Call* item)
{
   Q_UNUSED(item)
   return false;
}

bool LegacyHistoryBackend::save(const Call* call)
{
   Q_UNUSED(call)
   return false;
}

AbstractHistoryBackend::SupportedFeatures LegacyHistoryBackend::supportedFeatures() const
{
   return (AbstractHistoryBackend::SupportedFeatures) (
      AbstractHistoryBackend::SupportedFeatures::LOAD );
}

///Edit 'item', the implementation may be a GUI or somehting else
bool LegacyHistoryBackend::edit( Call* call)
{
   Q_UNUSED(call)
   return false;
}
///Add a new item to the backend
bool LegacyHistoryBackend::addNew( Call* call)
{
   Q_UNUSED(call)
   return true;
}

///Add a new phone number to an existing item
bool LegacyHistoryBackend::addPhoneNumber( Call* call , PhoneNumber* number )
{
   Q_UNUSED(call)
   Q_UNUSED(number)
   return false;
}

QByteArray LegacyHistoryBackend::id() const
{
   return "lhb";
}

QList<Call*> LegacyHistoryBackend::items() const
{
   return QList<Call*>();
}
