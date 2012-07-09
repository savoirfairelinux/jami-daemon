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
#include "VideoRenderer.h"

#include <QtCore/QDebug>

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>

#include <QtCore/QTimer>

///Shared memory object
struct SHMHeader {
    sem_t notification;
    sem_t mutex;

    unsigned m_BufferGen;
    int m_BufferSize;

    char m_Data[0];
};

///Constructor
VideoRenderer::VideoRenderer(QString shmPath, Resolution res): QObject(0),
   m_Width(0), m_Height(0), m_ShmPath(QString()), fd(-1),
   m_pShmArea((SHMHeader*)MAP_FAILED), m_ShmAreaLen(0), m_BufferGen(0),
   m_isRendering(false),m_pTimer(nullptr),m_Res(res)
{
   m_ShmPath = shmPath    ;
   m_Width   = res.width  ;
   m_Height  = res.height ;
}

///Destructor
VideoRenderer::~VideoRenderer()
{
   stopShm();
   //delete m_pShmArea;
}

///Get the data from shared memory and transform it into a QByteArray
QByteArray VideoRenderer::renderToBitmap(QByteArray& data,bool& ok)
{
   if (!m_isRendering) {
      return QByteArray();
   }

   if (!shmLock()) {
      ok = false;
      return QByteArray();
   }

   // wait for a new buffer
   while (m_BufferGen == m_pShmArea->m_BufferGen) {
      shmUnlock();
      const struct timespec timeout = createTimeout();
      int err = sem_timedwait(&m_pShmArea->notification, &timeout);
      // Useful for debugging
//       switch (errno ) {
//          case EINTR:
//             qDebug() << "Unlock failed: Interrupted function call (POSIX.1); see signal(7)";
//             ok = false;
//             return QByteArray();
//             break;
//          case EINVAL:
//             qDebug() << "Unlock failed: Invalid argument (POSIX.1)";
//             ok = false;
//             return QByteArray();
//             break;
//          case EAGAIN:
//             qDebug() << "Unlock failed: Resource temporarily unavailable (may be the same value as EWOULDBLOCK) (POSIX.1)";
//             ok = false;
//             return QByteArray();
//             break;
//          case ETIMEDOUT:
//             qDebug() << "Unlock failed: Connection timed out (POSIX.1)";
//             ok = false;
//             return QByteArray();
//             break;
//       }
      if (err < 0) {
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
   memcpy(data.data(),m_pShmArea->m_Data,m_pShmArea->m_BufferSize);
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
   timeout.tv_sec += TIMEOUT_SEC;
   return timeout;
}


/*****************************************************************************
 *                                                                           *
 *                                   Slots                                   *
 *                                                                           *
 ****************************************************************************/

///Update the buffer
void VideoRenderer::timedEvents()
{
   bool ok = true;
   QByteArray ba;
   renderToBitmap(m_Frame,ok);
   if (ok == true) {
      qDebug() << "Emit";
      emit frameUpdated();
   }
   else {
      qDebug() << "Frame dropped";
      usleep(rand()%100000); //Be sure it can come back in sync
   }
}

///Start the rendering loop
void VideoRenderer::startRendering()
{
   startShm();
   if (!m_pTimer) {
      m_pTimer = new QTimer(this);
      connect(m_pTimer,SIGNAL(timeout()),this,SLOT(timedEvents()));
      m_pTimer->setInterval(42);
   }
   m_pTimer->start();
   m_isRendering = true;
}

///Stop the rendering loop
void VideoRenderer::stopRendering()
{
   m_isRendering = false;
   stopShm();
   //qDebug() << "Video stopped for call" << id;
   //emit videoStopped();
   if (m_pTimer)
      m_pTimer->stop();
}


/*****************************************************************************
 *                                                                           *
 *                                 Getters                                   *
 *                                                                           *
 ****************************************************************************/

///Get the raw bytes directly from the SHM, not recommanded, but optimal
const char* VideoRenderer::rawData()
{
   return m_pShmArea->m_Data;
}

///Is this redenrer active
bool VideoRenderer::isRendering()
{
   return m_isRendering;
}

///Return the current framerate
QByteArray VideoRenderer::getCurrentFrame()
{
   return m_Frame;
}

///Return the current resolution
Resolution VideoRenderer::getActiveResolution()
{
   return m_Res;
}

/*****************************************************************************
 *                                                                           *
 *                                 Setters                                   *
 *                                                                           *
 ****************************************************************************/

void VideoRenderer::setResolution(QSize size)
{
   m_Res = size;
   m_Width = size.width();
   m_Height = size.height();
}

void VideoRenderer::setShmPath(QString path)
{
   m_ShmPath = path;
}