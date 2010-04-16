/***************************************************************************
 *   Copyright (C) 2009-2010 by Savoir-Faire Linux                         *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "Codec.h"

#include "configurationmanager_interface_singleton.h"
#include "sflphone_const.h"

Codec::Codec(int payload, bool enabled)
{
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   QStringList details = configurationManager.getCodecDetails(payload);
   this->payload = QString::number(payload);
   this->enabled = enabled;
   this->name = details[CODEC_NAME];
   this->frequency = details[CODEC_SAMPLE_RATE];
   this->bitrate = details[CODEC_BIT_RATE];
   this->bandwidth = details[CODEC_BANDWIDTH];
}

QString Codec::getPayload() const
{
  return payload;
}

QString Codec::getName() const
{
  return name;
}

QString Codec::getFrequency() const
{
  return frequency;
}

QString Codec::getBitrate() const
{
  return bitrate;
}

QString Codec::getBandwidth() const
{
  return bandwidth;
}

bool Codec::isEnabled() const
{
  return enabled;
}

void Codec::setPayload(QString payload)
{
  this->payload = payload;
}

void Codec::setName(QString name)
{
  this->name = name;
}

void Codec::setFrequency(QString frequency)
{
  this->frequency = frequency;
}

void Codec::setBitrate(QString bitrate)
{
  this->bitrate = bitrate;
}

void Codec::setBandwidth(QString bandwidth)
{
  this->bandwidth = bandwidth;
}

void Codec::setEnabled(bool enabled)
{
  this->enabled = enabled;
}


