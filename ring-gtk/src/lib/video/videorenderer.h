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
#ifndef VIDEO_RENDERER_H
#define VIDEO_RENDERER_H

//Base
#include <QtCore/QObject>
#include <QtCore/QTime>
#include "../typedefs.h"
#include <time.h>

//Qt
class QTimer;
class QMutex;

//SFLPhone
#include "videodevice.h"
struct SHMHeader;

///Manage shared memory and convert it to QByteArray
class LIB_EXPORT VideoRenderer : public QObject {
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop

   public:
      //Constructor
      VideoRenderer (const QString& id, const QString& shmPath, const QSize& res);
      ~VideoRenderer();

      //Mutators
      bool resizeShm();
      void stopShm  ();
      bool startShm ();

      //Getters
      const char*       rawData         ()      ;
      bool              isRendering     ()      ;
      const QByteArray& currentFrame    ()      ;
      QSize             size            ()      ;
      QMutex*           mutex           ()      ;
      int               fps             () const;

      //Setters
      void setSize(const QSize& res);
      void setShmPath   (const QString& path);

   private:
      //Attributes
      QString           m_ShmPath    ;
      int               fd           ;
      SHMHeader      *  m_pShmArea   ;
      signed int        m_ShmAreaLen ;
      uint              m_BufferGen  ;
      bool              m_isRendering;
      QTimer*           m_pTimer     ;
      QByteArray        m_Frame[2]   ;
      bool              m_FrameIdx   ;
      QSize             m_pSize      ;
      QMutex*           m_pMutex     ;
      QMutex*           m_pSSMutex   ;
      QString           m_Id         ;
      int               m_fpsC       ;
      int               m_Fps        ;
      QTime             m_CurrentTime;

      //Constants
      static const int TIMEOUT_SEC = 1; // 1 second

      //Helpers
      timespec createTimeout();
      bool     shmLock      ();
      void     shmUnlock    ();
      bool     renderToBitmap();

   private Q_SLOTS:
      void timedEvents();

   public Q_SLOTS:
      void startRendering();
      void stopRendering ();

   Q_SIGNALS:
      ///Emitted when a new frame is ready
      void frameUpdated();
      void stopped();
      void started();

};

#endif
