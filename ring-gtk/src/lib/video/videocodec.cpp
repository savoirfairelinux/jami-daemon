/****************************************************************************
 *   Copyright (C) 2012-2014 by Savoir-Faire Linux                          *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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
#include "videocodec.h"

#include <account.h>

QHash<QString,VideoCodec*> VideoCodec::m_slCodecs;
bool VideoCodec::m_sInit = false;

///Private constructor
VideoCodec::VideoCodec(const QString &codecName, uint bitRate, bool enabled) : QObject(),
m_Name(codecName),m_Bitrate(bitRate),m_Enabled(enabled)
{
   setObjectName("VideoCodec"+codecName);
}

///Get the current codec name
QString VideoCodec::name() const
{
   return m_Name;
}

///Get the current codec id
uint VideoCodec::bitrate() const
{
   return m_Bitrate;
}

///Get the current codec id
bool VideoCodec::isEnabled() const
{
   return m_Enabled;
}

///Set the codec bitrate
void VideoCodec::setBitrate(const uint bitrate)
{
   m_Bitrate = bitrate;
}

///Set if the codec is enabled
void VideoCodec::setEnabled(const bool enabled)
{
   m_Enabled = enabled;
}

///Set codec parameters
void VideoCodec::setParamaters(const QString& params )
{
   m_Parameters = params;
}

///Get codec parameters
QString VideoCodec::parameters() const
{
   return m_Parameters;
}

///Generate a daemon compatible codec representation
QMap<QString,QString> VideoCodec::toMap() const
{
   QMap<QString,QString> ret;
   ret[CodecFields::ENABLED    ] = isEnabled ()?"true":"false";
   ret[CodecFields::BITRATE    ] = QString::number(bitrate());
   ret[CodecFields::NAME       ] = name      ();
   ret[CodecFields::PARAMETERS ] = parameters();
   return ret;
}
