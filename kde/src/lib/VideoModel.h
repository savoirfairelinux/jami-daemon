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
#include "typedefs.h"
#include <QtCore/QObject>

//SFLPhone
#include "VideoDevice.h"

///@class VideoModel Video event dispatcher
class LIB_EXPORT VideoModel : public QObject {
public:
   VideoModel();
   void stopPreview();
   void startPreview();
   void setBufferSize(uint size);

private:
   //Attributes
   uint m_BufferSize;
   uint m_ShmKey;
   uint m_SemKey;
   Resolution m_Res;

private slots:
   void receivingEvent(int shmKey, int semKey, int videoBufferSize, int destWidth, int destHeight);
   void stoppedReceivingEvent(int shmKey, int semKey);
   void deviceEvent();
};