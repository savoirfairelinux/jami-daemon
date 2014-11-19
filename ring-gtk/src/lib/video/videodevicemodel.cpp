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
#include "videodevicemodel.h"
#include "videodevice.h"
#include <call.h>
#include <account.h>
#include "videocodec.h"
#include "../dbus/videomanager.h"

#include <QtCore/QCoreApplication>

VideoDeviceModel* VideoDeviceModel::m_spInstance = nullptr;

VideoDeviceModel* VideoDeviceModel::instance()
{
   if (!m_spInstance)
      m_spInstance = new VideoDeviceModel();
   return m_spInstance;
}

///Get data from the model
QVariant VideoDeviceModel::data( const QModelIndex& idx, int role) const
{
   if(idx.column() == 0 && role == Qt::DisplayRole)
      return QVariant(m_lDevices[idx.row()]->id());
   return QVariant();
}

///The number of codec
int VideoDeviceModel::rowCount( const QModelIndex& par ) const
{
   Q_UNUSED(par)
   return m_lDevices.size();
}

///Items flag
Qt::ItemFlags VideoDeviceModel::flags( const QModelIndex& idx ) const
{
   if (idx.column() == 0)
      return QAbstractItemModel::flags(idx) | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
   return QAbstractItemModel::flags(idx);
}

///Set the codec data (codecs can't be added or removed that way)
bool VideoDeviceModel::setData(const QModelIndex& idx, const QVariant &value, int role)
{
   Q_UNUSED(idx)
   Q_UNUSED(value)
   Q_UNUSED(role)
   return false;
}

///Constructor
VideoDeviceModel::VideoDeviceModel() : QAbstractListModel(QCoreApplication::instance()),
m_pDummyDevice(nullptr),m_pActiveDevice(nullptr)
{
   m_spInstance = this;
   reload();
}

///Destructor
VideoDeviceModel::~VideoDeviceModel()
{
   while (m_lDevices.size()) {
      VideoDevice* c = m_lDevices[0];
      m_lDevices.removeAt(0);
      delete c;
   }
}

///Save the current model over dbus
void VideoDeviceModel::setActive(const QModelIndex& idx)
{
   if (idx.isValid()) {
      VideoManagerInterface& interface = DBus::VideoManager::instance();
      interface.setDefaultDevice(m_lDevices[idx.row()]->id());
      m_pActiveDevice = m_lDevices[idx.row()];
      emit changed();
      emit currentIndexChanged(idx.row());
   }
}

///Convenience
void VideoDeviceModel::setActive(const int idx)
{
   setActive(index(idx,0,QModelIndex()));
}


void VideoDeviceModel::setActive(const VideoDevice* device)
{
   VideoManagerInterface& interface = DBus::VideoManager::instance();

   interface.setDefaultDevice(device?device->id():VideoDevice::NONE);
   m_pActiveDevice = const_cast<VideoDevice*>(device);
   emit changed();
   const int idx = m_lDevices.indexOf((VideoDevice*)device);
   emit currentIndexChanged(idx);
}

void VideoDeviceModel::reload()
{
   QHash<QString,VideoDevice*> devicesHash;
   VideoManagerInterface& interface = DBus::VideoManager::instance();
   const QStringList deviceList = interface.getDeviceList();
   if (deviceList.size() == m_hDevices.size()) {
      m_lDevices = m_hDevices.values();
   }

   foreach(const QString& deviceName,deviceList) {
      if (!m_hDevices[deviceName]) {
         devicesHash[deviceName] = new VideoDevice(deviceName);
      }
      else {
         devicesHash[deviceName] = m_hDevices[deviceName];
      }
   }
   foreach(VideoDevice* dev,m_hDevices) {
      if (dev && devicesHash.key(dev).isEmpty()) {
         delete dev;
      }
   }
   m_hDevices.clear();
   m_hDevices = devicesHash;
   m_lDevices = m_hDevices.values();

   emit layoutChanged();
//    channelModel   ()->reload();
   setActive(activeDevice());
}


VideoDevice* VideoDeviceModel::activeDevice() const
{
   if (!m_pActiveDevice) {
      VideoManagerInterface& interface = DBus::VideoManager::instance();
      const QString deId = interface.getDefaultDevice();
      if (!m_lDevices.size())
         const_cast<VideoDeviceModel*>(this)->reload();
      VideoDevice* dev =  m_hDevices[deId];

      //Handling null everywhere is too long, better create a dummy device and
      //log the event
      if (!dev) {
         if (!deId.isEmpty())
            qWarning() << "Requested unknown device" << deId;
         if (!m_pDummyDevice)
            const_cast<VideoDeviceModel*>(this)->m_pDummyDevice = new VideoDevice("None");
         return m_pDummyDevice;
      }
      const_cast<VideoDeviceModel*>(this)->m_pActiveDevice = dev;
   }
   return m_pActiveDevice;
}


int VideoDeviceModel::activeIndex() const
{
   return m_lDevices.indexOf(activeDevice());
}


// Extended Device list


ExtendedVideoDeviceModel* ExtendedVideoDeviceModel::m_spInstance = nullptr;

ExtendedVideoDeviceModel::ExtendedVideoDeviceModel() : QAbstractListModel(QCoreApplication::instance()),
m_CurrentSelection(-1)
{
   m_Display.rect = QRect(0,0,0,0);
}

ExtendedVideoDeviceModel* ExtendedVideoDeviceModel::instance()
{
   if (!m_spInstance)
      m_spInstance = new ExtendedVideoDeviceModel();
   return m_spInstance;
}

QVariant ExtendedVideoDeviceModel::data( const QModelIndex& index, int role ) const
{
   switch (index.row()) {
      case ExtendedDeviceList::NONE:
         switch(role) {
            case Qt::DisplayRole:
               return tr("NONE");
         };
         break;
      case ExtendedDeviceList::SCREEN:
         switch(role) {
            case Qt::DisplayRole:
               return tr("SCREEN");
         };
         break;
      case ExtendedDeviceList::FILE:
         switch(role) {
            case Qt::DisplayRole:
               return tr("FILE");
         };
         break;
      default:
         return VideoDeviceModel::instance()->data(VideoDeviceModel::instance()->index(index.row()-ExtendedDeviceList::__COUNT,0),role);
   };
   return QVariant();
}

int ExtendedVideoDeviceModel::rowCount( const QModelIndex& parent ) const
{
   Q_UNUSED(parent)
   return VideoDeviceModel::instance()->rowCount() + ExtendedDeviceList::__COUNT;
}

Qt::ItemFlags ExtendedVideoDeviceModel::flags( const QModelIndex& idx ) const
{
   switch (idx.row()) {
      case ExtendedDeviceList::NONE  :
      case ExtendedDeviceList::SCREEN:
      case ExtendedDeviceList::FILE  :
         return QAbstractItemModel::flags(idx) | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
         break;
      default:
         return VideoDeviceModel::instance()->flags(VideoDeviceModel::instance()->index(idx.row()-ExtendedDeviceList::__COUNT,0));
   };
}

bool ExtendedVideoDeviceModel::setData( const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED(index)
   Q_UNUSED(value)
   Q_UNUSED(role)
   return false;
}

void ExtendedVideoDeviceModel::switchTo(const QModelIndex& idx)
{
   switchTo(idx.row());
}

///This model is designed for "live" switching rather than configuration
void ExtendedVideoDeviceModel::switchTo(const int idx)
{
   switch (idx) {
      case ExtendedDeviceList::NONE:
         DBus::VideoManager::instance().switchInput(ProtocolPrefix::NONE);
         break;
      case ExtendedDeviceList::SCREEN:
         DBus::VideoManager::instance().switchInput( QString(ProtocolPrefix::DISPLAY)+QString(":%1 %2x%3")
            .arg(m_Display.index)
            .arg(m_Display.rect.width())
            .arg(m_Display.rect.height()));
         break;
      case ExtendedDeviceList::FILE:
         DBus::VideoManager::instance().switchInput(
            !m_CurrentFile.isEmpty()?+ProtocolPrefix::FILE+m_CurrentFile.path():ProtocolPrefix::NONE
         );
         break;
      default:
         DBus::VideoManager::instance().switchInput(ProtocolPrefix::V4L2 +
            VideoDeviceModel::instance()->index(idx-ExtendedDeviceList::__COUNT,0).data(Qt::DisplayRole).toString());
         break;
   };
   m_CurrentSelection = (ExtendedDeviceList) idx;
}

void ExtendedVideoDeviceModel::switchTo(VideoDevice* device)
{
   DBus::VideoManager::instance().switchInput(ProtocolPrefix::V4L2 + device->id());
}

VideoDevice* ExtendedVideoDeviceModel::deviceAt(const QModelIndex& idx) const
{
   if (!idx.isValid()) return nullptr;
   switch (idx.row()) {
      case ExtendedDeviceList::NONE:
      case ExtendedDeviceList::SCREEN:
      case ExtendedDeviceList::FILE:
         return nullptr;
      default:
         return VideoDeviceModel::instance()->devices()[idx.row()-ExtendedDeviceList::__COUNT];
   };
}

int ExtendedVideoDeviceModel::activeIndex() const
{
   if (m_CurrentSelection == -1) {
      return ExtendedDeviceList::__COUNT + VideoDeviceModel::instance()->activeIndex();
   }
   return m_CurrentSelection;
}

void ExtendedVideoDeviceModel::setFile(const QUrl& url)
{
   m_CurrentFile = url;
   switchTo(ExtendedDeviceList::FILE);
}

void ExtendedVideoDeviceModel::setDisplay(int index, QRect rect)
{
   m_Display.index  = index ;
   m_Display.rect   = rect  ;
   switchTo(ExtendedDeviceList::SCREEN);
}

VideoDevice* VideoDeviceModel::getDevice(const QString& devId) const
{
   return m_hDevices[devId];
}

QList<VideoDevice*> VideoDeviceModel::devices() const
{
   return m_lDevices;
}
