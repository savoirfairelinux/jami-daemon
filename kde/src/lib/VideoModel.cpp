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
#include "VideoModel.h"

//SFLPhone
#include "video_interface_singleton.h"
#include "VideoDevice.h"
#include "Call.h"
#include "CallModel.h"
#include "VideoRenderer.h"

//Static member
VideoModel* VideoModel::m_spInstance = nullptr;

///Constructor
VideoModel::VideoModel():m_BufferSize(0),m_ShmKey(0),m_SemKey(0),m_PreviewState(false)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   connect( &interface , SIGNAL(deviceEvent())                           , this, SLOT(deviceEvent())                           );
   connect( &interface , SIGNAL(startedDecoding(QString,QString,int,int)), this, SLOT(startedDecoding(QString,QString,int,int)));
   connect( &interface , SIGNAL(stoppedDecoding(QString,QString))        , this, SLOT(stoppedDecoding(QString,QString))        );
}

///Singleton
VideoModel* VideoModel::getInstance()
{
   if (!m_spInstance) {
      m_spInstance = new VideoModel();
   }
   return m_spInstance;
}

///Return the call renderer or nullptr
VideoRenderer* VideoModel::getRenderer(Call* call)
{
   if (!call) return nullptr;
   return m_lRenderers[call->getCallId()];
}

///Get the video preview renderer
VideoRenderer* VideoModel::getPreviewRenderer()
{
   if (!m_lRenderers["local"]) {
      VideoInterface& interface = VideoInterfaceSingleton::getInstance();
      m_lRenderers["local"] = new VideoRenderer("", Resolution(interface.getActiveDeviceSize()));
   }
   return m_lRenderers["local"];
}

///Stop video preview
void VideoModel::stopPreview()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.stopPreview();
   m_PreviewState = false;
}

///Start video preview
void VideoModel::startPreview()
{
   if (m_PreviewState) return;
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.startPreview();
   m_PreviewState = true;
}

///Is the video model fetching preview from a camera
bool VideoModel::isPreviewing()
{
   return m_PreviewState;
}

///@todo Set the video buffer size
void VideoModel::setBufferSize(uint size)
{
   m_BufferSize = size;
}

///Event callback
void VideoModel::deviceEvent()
{
   
}

///Return the current resolution
// Resolution VideoModel::getActiveResolution()
// {
//    return m_Res;
// }

///A video is not being rendered
void VideoModel::startedDecoding(QString id, QString shmPath, int width, int height)
{
   Q_UNUSED(id)
   qDebug() << "PREVIEW ID" << id;
//    m_pRenderer->m_ShmPath = shmPath;
//    m_Res.width            = width  ;
//    m_Res.height           = height ;
//    m_pRenderer->m_Width   = width  ;
//    m_pRenderer->m_Height  = height ;
//    m_pRenderer->m_isRendering = true;
//    m_pRenderer->startShm();
   
//    if (!m_pTimer) {
//       m_pTimer = new QTimer(this);
//       connect(m_pTimer,SIGNAL(timeout()),this,SLOT(timedEvents()));
//       m_pTimer->setInterval(42);
//    }
//    m_pTimer->start();
   
   if (m_lRenderers[id] == nullptr ) {
      m_lRenderers[id] = new VideoRenderer(shmPath,Resolution(width,height));
   }
   else {
      VideoRenderer* renderer = m_lRenderers[id];
      renderer->setShmPath(shmPath);
      renderer->setResolution(QSize(width,height));
   }

//    if (!m_pRenderer)
//       m_pRenderer = m_lRenderers[id];
   
    m_lRenderers[id]->startRendering();
   if (id != "local") {
      qDebug() << "Starting video for call" << id;
      emit videoCallInitiated(m_lRenderers[id]);
   }
}

///A video stopped being rendered
void VideoModel::stoppedDecoding(QString id, QString shmPath)
{
   Q_UNUSED(shmPath)
   if ( m_lRenderers[id] )
       m_lRenderers[id]->stopRendering();
//    m_pRenderer->m_isRendering = false;
//    m_pRenderer->stopShm();
   qDebug() << "Video stopped for call" << id;
   emit videoStopped();
//    if (m_pTimer)
//       m_pTimer->stop();
}
