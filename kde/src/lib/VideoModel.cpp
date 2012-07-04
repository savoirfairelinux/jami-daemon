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
#include "VideoModel.h"

//Posix
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>

//SFLPhone
#include "video_interface_singleton.h"
#include "VideoDevice.h"

//Static member
VideoModel* VideoModel::m_spInstance = NULL;

///Shared memory object
struct SHMHeader {
    sem_t notification;
    sem_t mutex;

    unsigned m_BufferGen;
    int m_BufferSize;

    char m_Data[0];
};


///Manage shared memory and convert it to QByteArray
class VideoRenderer {
   //This class is private there is no point to make accessors
   friend class VideoModel;
   
   private:
      //Attributes
      uint       m_Width     ;
      uint       m_Height    ;
      QString    m_ShmPath   ;
      int        fd          ;
      SHMHeader* m_pShmArea  ;
      signed int m_ShmAreaLen;
      uint       m_BufferGen ;

      //Constants
      static const int TIMEOUT_NSEC = 10E8; // 1 second

      //Helpers
      timespec createTimeout();
      bool     shmLock      ();
      void     shmUnlock    ();


   public:
      //Constructor
      VideoRenderer ();
      ~VideoRenderer();

      //Mutators
      bool resizeShm();
      void stopShm  ();
      bool startShm ();

      //Getters
      QByteArray renderToBitmap(QByteArray& data, bool& ok);
};

///Constructor
VideoRenderer::VideoRenderer():
   m_Width(0), m_Height(0), m_ShmPath(QString()), fd(-1),
   m_pShmArea((SHMHeader*)MAP_FAILED), m_ShmAreaLen(0), m_BufferGen(0)
{
   
}

///Destructor
VideoRenderer::~VideoRenderer()
{
   stopShm();
   delete m_pShmArea;
}

///Get the data from shared memory and transform it into a QByteArray
QByteArray VideoRenderer::renderToBitmap(QByteArray& data,bool& ok)
{
   if (!shmLock()) {
      ok = false;
      return QByteArray();
   }

   // wait for a new buffer
   while (m_BufferGen == m_pShmArea->m_BufferGen) {
      shmUnlock();
      const struct timespec timeout = createTimeout();
      // Could not decrement semaphore in time, returning
      if (sem_timedwait(&m_pShmArea->notification, &timeout) < 0) {
         ok = false;
         return QByteArray();
      }

      if (!shmLock()) {
         ok = false;
         return QByteArray();
      }
   }

   if (!resizeShm()) {
      qDebug() << "Could not resize shared memory";
      ok = false;
      return QByteArray();
   }

   if (data.size() != m_pShmArea->m_BufferSize)
      data.resize(m_pShmArea->m_BufferSize);
   //data = m_pShmArea->m_Data;
   memcpy(data.data(),m_pShmArea->m_Data,m_pShmArea->m_BufferSize);
   QByteArray data2(m_pShmArea->m_Data,m_pShmArea->m_BufferSize);
//    QByteArray data3(m_pShmArea->m_Data,m_pShmArea->m_BufferSize);
//    QByteArray data4(m_pShmArea->m_Data,m_pShmArea->m_BufferSize);
//    QByteArray data5(m_pShmArea->m_Data,m_pShmArea->m_BufferSize);
   m_BufferGen = m_pShmArea->m_BufferGen;
   shmUnlock();
   return data;
}

///Connect to the shared memory
bool VideoRenderer::startShm()
{
   if (fd != -1) {
      qDebug() << "fd must be -1";
      return false;
   }

   fd = shm_open(m_ShmPath.toAscii(), O_RDWR, 0);
   if (fd < 0) {
      qDebug() << "could not open shm area \"%s\", shm_open failed:%s" << m_ShmPath << strerror(errno);
      return false;
   }
   m_ShmAreaLen = sizeof(SHMHeader);
   m_pShmArea = (SHMHeader*) mmap(NULL, m_ShmAreaLen, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (m_pShmArea == MAP_FAILED) {
      qDebug() << "Could not map shm area, mmap failed";
      return false;
   }
   return true;
}

///Disconnect from the shared memory
void VideoRenderer::stopShm()
{
   if (fd >= 0)
      close(fd);
   fd = -1;

   if (m_pShmArea != MAP_FAILED)
      munmap(m_pShmArea, m_ShmAreaLen);
   m_ShmAreaLen = 0;
   m_pShmArea = (SHMHeader*) MAP_FAILED;
}

///Resize the shared memory
bool VideoRenderer::resizeShm()
{
   while ((sizeof(SHMHeader) + m_pShmArea->m_BufferSize) > m_ShmAreaLen) {
      const size_t new_size = sizeof(SHMHeader) + m_pShmArea->m_BufferSize;

      shmUnlock();
      if (munmap(m_pShmArea, m_ShmAreaLen)) {
            qDebug() << "Could not unmap shared area:%s" << strerror(errno);
            return false;
      }

      m_pShmArea = (SHMHeader*) mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      m_ShmAreaLen = new_size;

      if (!m_pShmArea) {
            m_pShmArea = 0;
            qDebug() << "Could not remap shared area";
            return false;
      }

      m_ShmAreaLen = new_size;
      if (!shmLock())
            return false;
   }
   return true;
}

///Lock the memory while the copy is being made
bool VideoRenderer::shmLock()
{
   const timespec timeout = createTimeout();
   /* We need an upper limit on how long we'll wait to avoid locking the whole GUI */
   if (sem_timedwait(&m_pShmArea->mutex, &timeout) == ETIMEDOUT) {
      qDebug() << "Timed out before shm lock was acquired";
      return false;
   }
   return true;
}

///Remove the lock, allow a new frame to be drawn
void VideoRenderer::shmUnlock()
{
   sem_post(&m_pShmArea->mutex);
}

///Create a SHM timeout
timespec VideoRenderer::createTimeout()
{
   timespec timeout = {0, 0};
   if (clock_gettime(CLOCK_REALTIME, &timeout) == -1)
      qDebug() << "clock_gettime";
   timeout.tv_nsec += TIMEOUT_NSEC;
   return timeout;
}

///Constructor
VideoModel::VideoModel():m_BufferSize(0),m_ShmKey(0),m_SemKey(0),m_Res(0,0),m_pTimer(0),m_PreviewState(false),m_pRenderer(new VideoRenderer()),
m_Attached(false)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   connect( &interface , SIGNAL(deviceEvent()                       ), this, SLOT( deviceEvent()                       ));
   connect( &interface , SIGNAL(startedDecoding(QString,QString,int,int)), this, SLOT( startedDecoding(QString,QString,int,int) ));
   connect( &interface , SIGNAL(stoppedDecoding(QString,QString)        ), this, SLOT( stoppedDecoding(QString,QString        ) ));
}

///Singleton
VideoModel* VideoModel::getInstance()
{
   if (!m_spInstance) {
      m_spInstance = new VideoModel();
   }
   return m_spInstance;
}

///Stop video preview
void VideoModel::stopPreview()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.stopPreview();
   m_pRenderer->stopShm();
   m_PreviewState = false;
   if (m_pTimer)
      m_pTimer->stop();
   if (m_Attached) {
      m_Attached = false;
   }
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

///Update the buffer
void VideoModel::timedEvents()
{
   bool ok = true;
   QByteArray ba;
   m_pRenderer->renderToBitmap(m_Frame,ok);
   if (ok == true)
      emit frameUpdated();
   else {
      qDebug() << "Frame dropped";
      usleep(rand()%100000); //Be sure it can come back in sync
   }
}

///Return the current framerate
QByteArray VideoModel::getCurrentFrame()
{
   return m_Frame;
}

///Return the current resolution
Resolution VideoModel::getActiveResolution()
{
   return m_Res;
}

///A video is not being rendered
void VideoModel::startedDecoding(QString id, QString shmPath, int width, int height)
{
   Q_UNUSED(id)
   m_pRenderer->m_ShmPath = shmPath;
   m_Res.width            = width  ;
   m_Res.height           = height ;
   m_pRenderer->m_Width   = width  ;
   m_pRenderer->m_Height  = height ;
   m_pRenderer->startShm();
   if (!m_pTimer) {
      m_pTimer = new QTimer(this);
      connect(m_pTimer,SIGNAL(timeout()),this,SLOT(timedEvents()));
   }
   m_pTimer->setInterval(42);
   m_pTimer->start();
}

///A video stopped being rendered
void VideoModel::stoppedDecoding(QString id, QString shmPath)
{
   Q_UNUSED(id)
   Q_UNUSED(shmPath)
}

char* VideoModel::rawData()
{
   return m_pRenderer->m_pShmArea->m_Data;
}