/******************************************************************************
 *   Copyright (C) 2012-2014 by Savoir-Faire Linux                            *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>   *
 *                                                                            *
 *   This library is free software; you can redistribute it and/or            *
 *   modify it under the terms of the GNU Lesser General Public               *
 *   License as published by the Free Software Foundation; either             *
 *   version 2.1 of the License, or (at your option) any later version.       *
 *                                                                            *
 *   This library is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU        *
 *   Lesser General Public License for more details.                          *
 *                                                                            *
 *   You should have received a copy of the Lesser GNU General Public License *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
 *****************************************************************************/
#ifndef VIDEO_MODEL_H
#define VIDEO_MODEL_H
//Base
#include "../typedefs.h"
#include <QtCore/QThread>

//Qt
#include <QtCore/QHash>

//SFLPhone
#include "videodevice.h"
class VideoRenderer;
class Call;
class QMutex;
struct SHMHeader;

///VideoModel: Video event dispatcher
class LIB_EXPORT VideoModel : public QThread {
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
public:
   //Singleton
   static VideoModel* instance();

   //Getters
   bool       isPreviewing       ();
   VideoRenderer* getRenderer(const Call* call) const;
   VideoRenderer* previewRenderer();
//    QList<VideoDevice*> devices();
//    VideoDevice* activeDevice() const;
//    VideoDevice* device(const QString &id);
   QMutex* startStopMutex() const;

   //Setters
   void setBufferSize(uint size);
//    void setActiveDevice(const VideoDevice* device);
   void switchDevice(const VideoDevice* device) const;

protected:
//    void run();

private:
   //Constructor
   VideoModel();
   ~VideoModel();

   //Static attributes
   static VideoModel* m_spInstance;

   //Attributes
   bool           m_PreviewState;
   uint           m_BufferSize  ;
   uint           m_ShmKey      ;
   uint           m_SemKey      ;
   QMutex*        m_SSMutex     ;
   QHash<QString,VideoRenderer*> m_lRenderers;
//    QHash<QString,VideoDevice*>   m_hDevices  ;

public Q_SLOTS:
   void stopPreview ();
   void startPreview();

private Q_SLOTS:
   void startedDecoding(const QString& id, const QString& shmPath, int width, int height);
   void stoppedDecoding(const QString& id, const QString& shmPath);
   void deviceEvent();

Q_SIGNALS:
   ///Emitted when a new frame is ready
//    void frameUpdated();
   ///Emmitted when the video is stopped, before the framebuffer become invalid
//    void videoStopped();
   ///Emmitted when a call make video available
   void videoCallInitiated(VideoRenderer*);
   ///The preview started/stopped
   void previewStateChanged(bool startStop);
   void previewStarted(VideoRenderer* renderer);
   void previewStopped(VideoRenderer* renderer);

};

#endif
