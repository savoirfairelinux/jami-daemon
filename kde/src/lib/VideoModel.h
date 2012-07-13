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
//Base
#include "typedefs.h"
#include <QtCore/QObject>

//Qt
#include <QtCore/QHash>

//SFLPhone
#include "VideoDevice.h"
class VideoRenderer;
class Call;
struct SHMHeader;

///VideoModel: Video event dispatcher
class LIB_EXPORT VideoModel : public QObject {
   Q_OBJECT
public:
   //Singleton
   static VideoModel* getInstance();

   //Getters
   bool       isPreviewing       ();
   VideoRenderer* getRenderer(Call* call);
   VideoRenderer* getPreviewRenderer();
   
   //Setters
   void       setBufferSize(uint size);

private:
   //Constructor
   VideoModel();

   //Static attributes
   static VideoModel* m_spInstance;
   
   //Attributes
   bool           m_Attached    ;
   bool           m_PreviewState;
   uint           m_BufferSize  ;
   uint           m_ShmKey      ;
   uint           m_SemKey      ;
   int            m_SetSetId    ;
   void*          m_pBuffer     ;
   QHash<QString,VideoRenderer*> m_lRenderers;

public slots:
   void stopPreview ();
   void startPreview();

private slots:
   void startedDecoding(QString id, QString shmPath, int width, int height);
   void stoppedDecoding(QString id, QString shmPath);
   void deviceEvent();

signals:
   ///Emitted when a new frame is ready
   void frameUpdated();
   ///Emmitted when the video is stopped, before the framebuffer become invalid
   void videoStopped();
   ///Emmitted when a call make video available
   void videoCallInitiated(QString callId);
};

#endif