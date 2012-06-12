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
#ifndef VIDEO_MODEL_H
#define VIDEO_MODEL_H

#include "typedefs.h"
#include <QtCore/QObject>

//Qt
class QSharedMemory;
class QTimer;

//SFLPhone
#include "VideoDevice.h"

///@class VideoModel Video event dispatcher
class LIB_EXPORT VideoModel : public QObject {
   Q_OBJECT
public:
   static VideoModel* getInstance();
   void setBufferSize(uint size);
   bool isPreviewing();
   QByteArray getCurrentFrame();
   Resolution getActiveResolution();

private:
   VideoModel();
   //Attributes
   static VideoModel* m_spInstance;
   bool m_Attached;
   bool m_PreviewState;
   uint m_BufferSize;
   uint m_ShmKey;
   uint m_SemKey;
   int  m_SetSetId;
   Resolution m_Res;
   QByteArray m_Frame;
   QTimer* m_pTimer;
   void* m_pBuffer;

public slots:
   void stopPreview();
   void startPreview();

private slots:
   void receivingEvent(int shmKey, int semKey, int videoBufferSize, int destWidth, int destHeight);
   void stoppedReceivingEvent(int shmKey, int semKey);
   void deviceEvent();
   void timedEvents();

signals:
   void frameUpdated();
};

/*class LIB_EXPORT VideoStream : public QThread {
public:
   VideoStream(QObject* parent) : QThread(parent) {

   }
   void run() {

   }
};*/

#endif