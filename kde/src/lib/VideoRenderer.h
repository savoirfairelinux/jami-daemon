/****************************************************************************
 *   Copyright (C) 2012 by Savoir-Faire Linux                               *
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
#include "typedefs.h"

//Qt
class QTimer;

//SFLPhone
#include "VideoDevice.h"
struct SHMHeader;

///Manage shared memory and convert it to QByteArray
class LIB_EXPORT VideoRenderer : public QObject {
   Q_OBJECT
   
   public:
      //Constructor
      VideoRenderer (QString shmPath,Resolution res);
      ~VideoRenderer();

      //Mutators
      bool resizeShm();
      void stopShm  ();
      bool startShm ();

      //Getters
      QByteArray  renderToBitmap(QByteArray& data, bool& ok);
      const char* rawData            ();
      bool        isRendering        ();
      QByteArray  getCurrentFrame    ();
      Resolution  getActiveResolution();

      //Setters
      void setResolution(QSize   size);
      void setShmPath   (QString path);

   private:
      //Attributes
      uint       m_Width      ;
      uint       m_Height     ;
      QString    m_ShmPath    ;
      int        fd           ;
      SHMHeader* m_pShmArea   ;
      signed int m_ShmAreaLen ;
      uint       m_BufferGen  ;
      bool       m_isRendering;
      QTimer*    m_pTimer     ;
      QByteArray m_Frame      ;
      Resolution m_Res        ;

      //Constants
      static const int TIMEOUT_SEC = 1; // 1 second

      //Helpers
      timespec createTimeout();
      bool     shmLock      ();
      void     shmUnlock    ();

   private slots:
      void timedEvents();

   public slots:
      void startRendering();
      void stopRendering ();

   signals:
      ///Emitted when a new frame is ready
      void frameUpdated();

};

#endif
