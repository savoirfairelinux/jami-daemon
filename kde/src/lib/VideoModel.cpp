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
#include <unistd.h>

//Qt
#include <QSharedMemory>
#include <QDBusPendingReply>

//SFLPhone
#include "video_interface_singleton.h"
#include "VideoDevice.h"

//Static member
VideoModel* VideoModel::m_spInstance = NULL;

///@namespace ShmManager Low level function to access shared memory
namespace ShmManager {
   ///Get the shared memory key
   static int getShm(unsigned numBytes, int shmKey)
   {
      key_t key = shmKey;
      int shm_id = shmget(key, numBytes, 0644);

      if (shm_id == -1)
         qDebug() << ("shmget");

      return shm_id;
   }

   ///Get the shared buffer
   static void * attachShm(int shm_id)
   {
      void *data = shmat(shm_id, (void *)0, 0);
      if (data == (char *)(-1)) {
         qDebug() << ("shmat");
         data = NULL;
      }

      return data;
   }

   ///Detach shared ownership of the buffer
   static void detachShm(char *data)
   {
      /* detach from the segment: */
      if (shmdt(data) == -1)
         qDebug() << ("shmdt");
   }

   #if _SEM_SEMUN_UNDEFINED
   union semun
   {
      int                 val  ; /* value for SETVAL */
      struct semid_ds*    buf  ; /* buffer for IPC_STAT & IPC_SET */
      unsigned short int* array; /* array for GETALL & SETALL */
      struct seminfo*     __buf; /* buffer for IPC_INFO */
   };
   #endif

   ///Get the sempahor key
   static int get_sem_set(int semKey)
   {
      int sem_set_id;
      key_t key = semKey;

      union semun sem_val;

      sem_set_id = semget(key, 1, 0600);
      if (sem_set_id == -1) {
         qDebug() << ("semget");
         return sem_set_id;
      }
      sem_val.val = 0;
      semctl(sem_set_id, 0, SETVAL, sem_val);
      return sem_set_id;
   }

   ///Is a new frame ready to be fetched
   static int sem_wait(int sem_set_id)
   {
      /* structure for semaphore operations.   */
      struct sembuf sem_op;

      /* wait on the semaphore, unless it's value is non-negative. */
      sem_op.sem_num = 0;
      sem_op.sem_op = -1;
      sem_op.sem_flg = IPC_NOWAIT;
      return semop(sem_set_id, &sem_op, 1);
   }
};

///Constructor
VideoModel::VideoModel():m_BufferSize(0),m_ShmKey(0),m_SemKey(0),m_Res(0,0),m_pTimer(0),m_PreviewState(false),
m_Attached(false)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   connect( &interface , SIGNAL(receivingEvent(int,int,int,int,int) ), this, SLOT( receivingEvent(int,int,int,int,int) ));
   connect( &interface , SIGNAL(deviceEvent()                       ), this, SLOT( deviceEvent()                       ));
   connect( &interface , SIGNAL(stoppedReceivingEvent(int,int)      ), this, SLOT( stoppedReceivingEvent(int,int)      ));
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
   m_PreviewState = false;
   if (m_pTimer)
      m_pTimer->stop();
   if (m_Attached) {
      ShmManager::detachShm((char*)m_pBuffer);
      m_Attached = false;
   }
}

///Start video preview
void VideoModel::startPreview()
{
   if (m_PreviewState) return;
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   QDBusPendingReply<int,int,int,int,int> reply = interface.startPreview();
   reply.waitForFinished();
   if (!reply.isError()) {
      m_Res.width   = reply.argumentAt(0).toInt();
      m_Res.height  = reply.argumentAt(1).toInt();
      m_ShmKey      = reply.argumentAt(2).toInt();
      m_SemKey      = reply.argumentAt(3).toInt();
      m_BufferSize  = reply.argumentAt(4).toInt();
      if (!m_pTimer) {
         m_pTimer = new QTimer(this);
         connect(m_pTimer,SIGNAL(timeout()),this,SLOT(timedEvents()));
      }
      m_pTimer->setInterval(42);
      m_pTimer->start();
      m_PreviewState = true;
   }
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
void VideoModel::receivingEvent(int shmKey, int semKey, int videoBufferSize, int destWidth, int destHeight)
{
   m_ShmKey     = (uint)shmKey   ;
   m_ShmKey     = (uint)semKey   ;
   m_BufferSize = videoBufferSize;
   m_Res.width  = destWidth      ;
   m_Res.height = destHeight     ;


}

///Callback when video is stopped
void VideoModel::stoppedReceivingEvent(int shmKey, int semKey)
{
   m_ShmKey = (uint)shmKey;
   m_ShmKey = (uint)semKey;
}

///Event callback
void VideoModel::deviceEvent()
{
   
}

///Update the buffer
void VideoModel::timedEvents()
{
   if ( !m_Attached ) {
      int shm_id = ShmManager::getShm(m_BufferSize, m_ShmKey);
      m_pBuffer  = ShmManager::attachShm(shm_id);
      m_Attached = true;
      m_SetSetId = ShmManager::get_sem_set(m_SemKey);
   }

   int ret = ShmManager::sem_wait(m_SetSetId);
   if (ret != -1) {
      QByteArray array((char*)m_pBuffer,m_BufferSize);
      m_Frame.resize(0);
      m_Frame = array;
      emit frameUpdated();
   }
   else {
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