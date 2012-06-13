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
#ifndef VIDEO_DEVICE_H
#define VIDEO_DEVICE_H

#include "typedefs.h"
#include <QStringList>

///@typedef VideoChannel A channel available in a Device
typedef QString VideoChannel;

///@typedef VideoRate The rate for a device
typedef QString VideoRate;

///@struct Resolution Equivalent of "640x480"
struct LIB_EXPORT Resolution {
   explicit Resolution(uint _width, uint _height):width(_width),height(_height){}
   Resolution(QString size) {
      width=size.split("x")[0].toInt();
      height=size.split("x")[1].toInt();
   }
   uint width;
   uint height;
   QString toString() { return QString::number(width)+"x"+QString::number(height);}
   bool operator==(const Resolution& other) {
      return (other.width == width && other.height == height);
   }
};

///@class VideoDevice V4L devices used to record video for video call
class LIB_EXPORT VideoDevice {
   public:
      static VideoDevice* getDevice(QString id);

      //Getter
      QStringList                getRateList(VideoChannel channel, Resolution resolution);
      QList<Resolution>          getResolutionList(VideoChannel channel);
      QList<VideoChannel>        getChannelList ();
      Resolution                 getResolution  ();
      VideoChannel               getChannel     ();
      VideoRate                  getRate        ();
      QString                    getDeviceId    ();
      
      //Static getter
      static QList<VideoDevice*> getDeviceList();

      //Setter
      void setRate       ( VideoRate rate        );
      void setResolution ( Resolution resolution );
      void setChannel    ( VideoChannel channel  );
   private:
      //Constructor
      VideoDevice(QString id);

      //Attributes
      QString m_DeviceId;
      static QHash<QString,VideoDevice*> m_slDevices;
      static bool m_sInit;
};
#endif