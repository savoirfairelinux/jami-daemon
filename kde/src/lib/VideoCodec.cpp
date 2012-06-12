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
#include "VideoCodec.h"
#include "Call.h"
#include "Account.h"
#include "video_interface_singleton.h"

QHash<QString,VideoCodec*> VideoCodec::m_slCodecs;
bool VideoCodec::m_sInit = false;

///Private constructor
VideoCodec::VideoCodec(QString codecName)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   QMap<QString,QString> details = interface.getCodecDetails(codecName);
   m_Name    = details["name"];//TODO do not use stringlist
   m_Bitrate = details["bitrate"];//TODO do not use stringlist
}

///Init the device list
void VideoCodec::init()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   QStringList codecs = interface.getCodecList();
   foreach(QString codec,codecs) {
      m_slCodecs[codec] = new VideoCodec(codec);
   }
   
   m_sInit = true;
}

///Get a codec from a name
VideoCodec* VideoCodec::getCodec(QString name)
{
   return m_slCodecs[name];
}

///Get the current call codec
//TODO move to call.h?
VideoCodec* VideoCodec::getCurrentCodec(Call* call)
{
   if (!m_sInit) init();
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return getCodec(interface.getCurrentCodecName(call->getCallId()));
}

///Get the complete video codec list
QList<VideoCodec*> VideoCodec::getCodecList()
{
   if (!m_sInit) init();
   return m_slCodecs.values();
}

///Get the list of active codecs
QList<VideoCodec*> VideoCodec::getActiveCodecList(Account* account)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   QStringList codecs = interface.getActiveCodecList(account->getAccountId());
   QList<VideoCodec*> toReturn;
   foreach(QString codec,codecs) {
      toReturn << getCodec(codec);
   }
   return toReturn;
}

///Set active codecs
void VideoCodec::setActiveCodecList(Account* account, QStringList codecs)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.setActiveCodecList(codecs,account->getAccountId());
}

///Get the current codec name
QString VideoCodec::getName()
{
   return m_Name;
}

///Get the current codec id
QString VideoCodec::getBitrate()
{
   return m_Bitrate;
}