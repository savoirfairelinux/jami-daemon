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
#ifndef VIDEO_DEVICE_H
#define VIDEO_DEVICE_H

#include "../typedefs.h"
#include <QtCore/QAbstractListModel>

//Qt
#include <QStringList>
#include <QtCore/QSize>

//SFLPhone
class VideoRenderer;
class VideoResolution;
class VideoRate;
class VideoChannel;
class VideoDevice;

class VideoModel;

///VideoDevice: V4L devices used to record video for video call
class LIB_EXPORT VideoDevice : public QAbstractListModel {
   Q_OBJECT
   friend class VideoModel;
   friend class VideoDeviceModel;

   //Need to access the PreferenceNames table
   friend class VideoChannel;
   friend class Resolution;
   public:

      class PreferenceNames {
      public:
         constexpr static const char* RATE    = "rate"   ;
         constexpr static const char* NAME    = "name"   ;
         constexpr static const char* CHANNEL = "channel";
         constexpr static const char* SIZE    = "size"   ;
      };

      //Constants
      constexpr static const char* NONE = "";

      //Model
      QVariant      data     ( const QModelIndex& index, int role = Qt::DisplayRole     ) const;
      int           rowCount ( const QModelIndex& parent = QModelIndex()                ) const;
      Qt::ItemFlags flags    ( const QModelIndex& index                                 ) const;
      virtual bool  setData  ( const QModelIndex& index, const QVariant &value, int role)      ;

      //Getter
      QList<VideoChannel*> channelList      () const;
      VideoChannel*        activeChannel    () const;
      const QString        id               () const;
      const QString        name             () const;
      bool  isActive                        () const;

      //Static getter

      //Setter
      bool setActiveChannel(VideoChannel* chan);
      bool setActiveChannel(int idx);

      //Mutator
      void save();

   private:
      //Constructor
      explicit VideoDevice(const QString &id);
      ~VideoDevice();

      //Attributes
      QString       m_DeviceId          ;
      VideoChannel* m_pCurrentChannel   ;
      QList<VideoChannel*> m_lChannels  ;


   Q_SIGNALS:
      void renderingStarted(VideoRenderer*);
      void renderingStopped(VideoRenderer*);
      void renderStateChanged(bool state);
};
#endif
