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
bool VideoDevice::m_sInit = false;

///Constructor
VideoDevice::VideoDevice(QString id) : m_DeviceId(id)
{
}

///Get the video device list
const QList<VideoDevice*> VideoDevice::getDeviceList()
{
   QHash<QString,VideoDevice*> devices;
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   const QStringList deviceList = interface.getDeviceList();
   foreach(const QString& device,deviceList) {
      if (!m_slDevices[device])
         devices[device] = new VideoDevice(device);
      else
         devices[device] = m_slDevices[device];
   }
   foreach(VideoDevice* dev,m_slDevices) {
      if (devices.key(dev).isEmpty())
         delete dev;
   }
   m_slDevices = devices;
   return m_slDevices.values();
}

///Return the device
VideoDevice* VideoDevice::getDevice(QString name)
{
   if (!m_sInit) getDeviceList();
   return m_slDevices[name];
}

///Get the valid rates for this device
const QStringList VideoDevice::getRateList(VideoChannel channel, Resolution resolution)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return interface.getDeviceRateList(m_DeviceId,channel,resolution.toString());
}

///Get the valid channel list
const QList<VideoChannel> VideoDevice::getChannelList()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return interface.getDeviceChannelList(m_DeviceId);
}

///Set the current device rate
void VideoDevice::setRate(VideoRate rate)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.setActiveDeviceRate(rate);
}

///Set the current resolution
void VideoDevice::setResolution(Resolution resolution) //??? No device
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.setActiveDeviceSize(resolution.toString());
}

///Set the current device channel
void VideoDevice::setChannel(VideoChannel channel) //??? No device
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.setActiveDeviceChannel(channel);
}

///Get the current resolution
const Resolution VideoDevice::getResolution()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return Resolution(interface.getActiveDeviceSize());
}

///Get the current channel
const VideoChannel VideoDevice::getChannel() //??? No device
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return interface.getActiveDeviceChannel();
}

///Get the current rate
const VideoRate VideoDevice::getRate()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   return interface.getActiveDeviceRate();
}

///Get a list of valid resolution
const QList<Resolution> VideoDevice::getResolutionList(VideoChannel channel)
{
   QList<Resolution> toReturn;
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   const QStringList list = interface.getDeviceSizeList(m_DeviceId,channel);
   foreach(const QString& res,list) {
      toReturn << Resolution(res);
   }
   return toReturn;
}

///Get the device id
const QString VideoDevice::getDeviceId() const
{
   return m_DeviceId;
}
