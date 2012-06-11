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

QHash<QString,VideoDevice*> VideoDevice::m_slDevices;


///Constructor
VideoDevice::VideoDevice(QString id) : m_DeviceId(id)
{
   m_slDevices[id] = this;
}

///Get the video device list
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

///Get the valid rates for this device
QStringList VideoDevice::getRateList(VideoChannel channel, Resolution resolution)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return interface.getInputDeviceRateList(m_DeviceId,channel,resolution.toString());
}

///Get the valid channel list
QList<VideoChannel> VideoDevice::getChannelList()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return interface.getInputDeviceChannelList(m_DeviceId);
}

///Set the current device rate
void VideoDevice::setRate(VideoRate rate)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.setInputDeviceRate(rate);
}

///Set the current resolution
void VideoDevice::setResolution(Resolution resolution) //??? No device
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.setInputDeviceSize(resolution.toString());
}

///Set the current device channel
void VideoDevice::setChannel(VideoChannel channel) //??? No device
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.setInputDeviceChannel(channel);
}

///Get the current resolution
Resolution VideoDevice::getResolution()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return Resolution(interface.getInputDeviceSize());
}

///Get the current channel
VideoChannel VideoDevice::getChannel() //??? No device
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return interface.getInputDeviceChannel();
}

///Get the current rate
VideoRate VideoDevice::getRate()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return interface.getInputDeviceRate();
}

///Get a list of valid resolution
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

QString VideoDevice::getDeviceId()
{
   return m_DeviceId;
}