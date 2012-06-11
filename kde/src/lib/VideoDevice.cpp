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
#include "VideoDevice.h"
#include "video_interface_singleton.h"

QList<VideoDevice*> VideoDevice::getDeviceList()
{
   QList<VideoDevice*> list;
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   QStringList deviceList = interface.getInputDeviceList();
   foreach(QString device,deviceList) {
      VideoDevice* dev = new VideoDevice(device);
      list << dev;
   }
   return list;
}

VideoDevice::VideoDevice(QString id) : m_DeviceId(id)
{
   
}

QStringList VideoDevice::getRateList(VideoChannel channel, Resolution resolution)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return interface.getInputDeviceRateList(m_DeviceId,channel,resolution.toString());
}

QList<VideoChannel> VideoDevice::getChannelList()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return interface.getInputDeviceChannelList(m_DeviceId);
}

void VideoDevice::setRate(VideoRate rate)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.setInputDeviceRate(rate);
}

void VideoDevice::setResolution(Resolution resolution) //??? No device
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.setInputDeviceSize(resolution.toString());
}

void VideoDevice::setChannel(VideoChannel channel) //??? No device
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.setInputDeviceChannel(channel);
}

Resolution VideoDevice::getResolution()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return Resolution(interface.getInputDeviceSize());
}

VideoChannel VideoDevice::getChannel() //??? No device
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return interface.getInputDeviceChannel();
}

VideoRate VideoDevice::getRate()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return interface.getInputDeviceRate();
}

QList<Resolution> VideoDevice::getResolutionList(VideoChannel channel)
{
   QList<Resolution> toReturn;
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   QStringList list = interface.getInputDeviceSizeList(m_DeviceId,channel);
   foreach(QString res,list) {
      toReturn << Resolution(res);
   }
   return toReturn;
}
