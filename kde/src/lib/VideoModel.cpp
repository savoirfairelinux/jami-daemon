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
//#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>

//Qt
#include <QSharedMemory>
#include <QDBusPendingReply>

//SFLPhone
#include "video_interface_singleton.h"
#include "VideoDevice.h"

//Static member
VideoModel* VideoModel::m_spInstance = NULL;

///@namespace ShmManager Low level function to access shared memory
// namespace ShmManager {
//    ///Get the shared memory key
//    static int getShm(unsigned numBytes, int shmKey)
//    {
//       key_t key = shmKey;
//       int shm_id = shmget(key, numBytes, 0644);
// 
//       if (shm_id == -1)
//          qDebug() << ("shmget");
// 
//       return shm_id;
//    }
// 
//    ///Get the shared buffer
//    static void * attachShm(int shm_id)
//    {
//       void *data = shmat(shm_id, (void *)0, 0);
//       if (data == (char *)(-1)) {
//          qDebug() << ("shmat");
//          data = NULL;
//       }
// 
//       return data;
//    }
// 
//    ///Detach shared ownership of the buffer
//    static void detachShm(char *data)
//    {
//       /* detach from the segment: */
//       if (shmdt(data) == -1)
//          qDebug() << ("shmdt");
//    }
// 
//    #if _SEM_SEMUN_UNDEFINED
//    union semun
//    {
//       int                 val  ; /* value for SETVAL */
//       struct semid_ds*    buf  ; /* buffer for IPC_STAT & IPC_SET */
//       unsigned short int* array; /* array for GETALL & SETALL */
//       struct seminfo*     __buf; /* buffer for IPC_INFO */
//    };
//    #endif
// 
//    ///Get the sempahor key
//    static int get_sem_set(int semKey)
//    {
//       int sem_set_id;
//       key_t key = semKey;
// 
//       union semun sem_val;
// 
//       sem_set_id = semget(key, 1, 0600);
//       if (sem_set_id == -1) {
//          qDebug() << ("semget");
//          return sem_set_id;
//       }
//       sem_val.val = 0;
//       semctl(sem_set_id, 0, SETVAL, sem_val);
//       return sem_set_id;
//    }
// 
//    ///Is a new frame ready to be fetched
//    static int sem_wait(int sem_set_id)
//    {
//       /* structure for semaphore operations.   */
//       struct sembuf sem_op;
// 
//       /* wait on the semaphore, unless it's value is non-negative. */
//       sem_op.sem_num = 0;
//       sem_op.sem_op = -1;
//       sem_op.sem_flg = IPC_NOWAIT;
//       return semop(sem_set_id, &sem_op, 1);
//    }
// };

//namespace ShmManager_v2 {
   enum
   {
      PROP_0,
      PROP_WIDTH,
      PROP_HEIGHT,
      PROP_DRAWAREA,
      PROP_SHM_PATH,
      PROP_LAST
   };


   
   /* Our private member structure */
//    struct _VideoRendererPrivate {
// 
//    };

typedef struct {
    sem_t notification;
    sem_t mutex;

    unsigned buffer_gen;
    int buffer_size;

    char data[0];
} SHMHeader;


class VideoRenderer {
   friend class VideoModel;
   private:
      uint width;
      uint height;
      QString shm_path;

//       ClutterActor *texture;
      //QImage texture;

      void* drawarea;
      int fd;
      SHMHeader* shm_area;
      signed int shm_area_len;
      uint buffer_gen;

      static const int TIMEOUT_NSEC = 10E8; // 1 second

      static timespec create_timeout()
      {
         timespec timeout = {0, 0};
         if (clock_gettime(CLOCK_REALTIME, &timeout) == -1)
            qDebug() << "clock_gettime";
         timeout.tv_nsec += TIMEOUT_NSEC;
         return timeout;
      }

      static bool shm_lock(SHMHeader *shm_area)
      {
         const timespec timeout = create_timeout();
         /* We need an upper limit on how long we'll wait to avoid locking the whole GUI */
         if (sem_timedwait(&shm_area->mutex, &timeout) == ETIMEDOUT) {
            qDebug() << "Timed out before shm lock was acquired";
            return FALSE;
         }
         return TRUE;
      }

      static void shm_unlock(SHMHeader *shm_area)
      {
         sem_post(&shm_area->mutex);
      }


   public:
      VideoRenderer() {
         //VideoRendererPrivate *priv;
         //self->priv = priv = VIDEO_RENDERER_GET_PRIVATE(self);
         width    = 0;
         height   = 0;
         shm_path = QString();
         //texture = NULL;
         drawarea = NULL;
         fd       = -1;
         shm_area = (SHMHeader*)MAP_FAILED;
         shm_area_len = 0;
         buffer_gen   = 0;
      }

      bool video_renderer_resize_shm()
      {
         while ((sizeof(SHMHeader) + shm_area->buffer_size) > shm_area_len) {
            const size_t new_size = sizeof(SHMHeader) + shm_area->buffer_size;

            shm_unlock(shm_area);
            if (munmap(shm_area, shm_area_len)) {
                  qDebug() << "Could not unmap shared area:%s" << strerror(errno);
                  return FALSE;
            }

            shm_area = (SHMHeader*) mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            shm_area_len = new_size;

            if (!shm_area) {
                  shm_area = 0;
                  qDebug() << "Could not remap shared area";
                  return FALSE;
            }

            shm_area_len = new_size;
            if (!shm_lock(shm_area))
                  return FALSE;
         }
         return TRUE;
      }

      void video_renderer_stop_shm()
      {
         //g_return_if_fail(IS_VIDEO_RENDERER(self));
         //VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(self);
         if (fd >= 0)
            close(fd);
         fd = -1;

         if (shm_area != MAP_FAILED)
            munmap(shm_area, shm_area_len);
         shm_area_len = 0;
         shm_area = (SHMHeader*) MAP_FAILED;
      }

      bool video_renderer_start_shm(/*VideoRenderer *self*/)
      {
         /* First test that 'self' is of the correct type */
         //g_return_val_if_fail(IS_VIDEO_RENDERER(self), FALSE);
         //VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(self);
         if (fd != -1) {
            qDebug() << "fd must be -1";
            return FALSE;
         }

         fd = shm_open(shm_path.toAscii(), O_RDWR, 0);
         if (fd < 0) {
            qDebug() << "could not open shm area \"%s\", shm_open failed:%s" << shm_path << strerror(errno);
            return FALSE;
         }
         shm_area_len = sizeof(SHMHeader);
         shm_area = (SHMHeader*) mmap(NULL, shm_area_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
         if (shm_area == MAP_FAILED) {
            qDebug() << "Could not map shm area, mmap failed";
            return FALSE;
         }
         return TRUE;
      }

      void video_renderer_finalize()
      {
         //VideoRenderer *self = VIDEO_RENDERER(obj);
         video_renderer_stop_shm();
         /* Chain up to the parent class */
         //G_OBJECT_CLASS(video_renderer_parent_class)->finalize(obj);
      }

      QByteArray video_renderer_render_to_texture(bool& ok)
      {
         if (!shm_lock(shm_area)) {
            ok = false;
            return QByteArray();
         }

         // wait for a new buffer
         while (buffer_gen == shm_area->buffer_gen) {
            shm_unlock(shm_area);
            const struct timespec timeout = create_timeout();
            // Could not decrement semaphore in time, returning
            if (sem_timedwait(&shm_area->notification, &timeout) < 0) {
               ok = false;
               return QByteArray();
            }

            if (!shm_lock(shm_area)) {
               ok = false;
               return QByteArray();
            }
         }

         if (!video_renderer_resize_shm()) {
            qDebug() << "Could not resize shared memory";
            ok = false;
            return QByteArray();
         }

         /*const int BPP = 4;
         const int ROW_STRIDE = BPP * width;
          update the clutter texture */
//             clutter_texture_set_from_rgb_data(CLUTTER_TEXTURE(texture),
//                      (guchar*) shm_area->data,
//                      TRUE,
//                      width,
//                      height,
//                      ROW_STRIDE,
//                      BPP,
//                      CLUTTER_TEXTURE_RGB_FLAG_BGR,
//                      NULL);
         QByteArray data(shm_area->data,shm_area->buffer_size);//((char*)m_pBuffer,m_BufferSize)
         buffer_gen = shm_area->buffer_gen;
         shm_unlock(shm_area);
         return data;
      }

//       bool render_frame_from_shm()
//       {
// //             if (!GTK_IS_WIDGET(drawarea))
// //                return FALSE;
// //             GtkWidget *parent = gtk_widget_get_parent(drawarea);
// //             if (!parent)
// //                return FALSE;
// //             const int parent_width = gtk_widget_get_allocated_width(parent);
// //             const int parent_height = gtk_widget_get_allocated_height(parent);
// //             clutter_actor_set_size(texture, parent_width, parent_height);
//          video_renderer_render_to_texture();
//          return TRUE;
//       }

      void video_renderer_stop(/*VideoRenderer *renderer*/)
      {
         //VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(renderer);
         //if (drawarea && GTK_IS_WIDGET(drawarea))
         //   gtk_widget_hide(GTK_WIDGET(drawarea));
         //g_object_unref(G_OBJECT(renderer));
      }

//       bool update_texture(void* data)
//       {
//          //VideoRenderer *renderer = (VideoRenderer *) data;
//          //VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(renderer);
//          const bool ret = render_frame_from_shm();
//          if (!ret) {
//             video_renderer_stop();
//             //g_object_unref(G_OBJECT(data));
//          }
//          return ret;
//       }
};


//}

///Constructor
VideoModel::VideoModel():m_BufferSize(0),m_ShmKey(0),m_SemKey(0),m_Res(0,0),m_pTimer(0),m_PreviewState(false),m_pRenderer(new VideoRenderer()),
m_Attached(false)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   //connect( &interface , SIGNAL(receivingEvent(int,int,int,int,int) ), this, SLOT( receivingEvent(int,int,int,int,int) ));
   connect( &interface , SIGNAL(deviceEvent()                       ), this, SLOT( deviceEvent()                       ));
   //connect( &interface , SIGNAL(stoppedReceivingEvent(int,int)      ), this, SLOT( stoppedReceivingEvent(int,int)      ));
   connect( &interface , SIGNAL(startedDecoding(QString,QString,int,int)      ), this, SLOT( startedDecoding(QString,QString,int,int) ));
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
   m_pRenderer->video_renderer_stop_shm();
   m_PreviewState = false;
   if (m_pTimer)
      m_pTimer->stop();
   if (m_Attached) {
      //ShmManager::detachShm((char*)m_pBuffer);
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
   //m_pRenderer->video_renderer_start_shm();
   //Open SHM
//    if (m_PreviewState) return;
//    VideoInterface& interface = VideoInterfaceSingleton::getInstance();
//    QDBusPendingReply<int,int,int,int,int> reply = interface.startPreview();
//    reply.waitForFinished();
//    if (!reply.isError()) {
//       m_Res.width   = reply.argumentAt(0).toInt();
//       m_Res.height  = reply.argumentAt(1).toInt();
//       m_ShmKey      = reply.argumentAt(2).toInt();
//       m_SemKey      = reply.argumentAt(3).toInt();
//       m_BufferSize  = reply.argumentAt(4).toInt();
//       if (!m_pTimer) {
//          m_pTimer = new QTimer(this);
//          connect(m_pTimer,SIGNAL(timeout()),this,SLOT(timedEvents()));
//       }
//       m_pTimer->setInterval(42);
//       m_pTimer->start();
//    }
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
// void VideoModel::receivingEvent(int shmKey, int semKey, int videoBufferSize, int destWidth, int destHeight)
// {
//    m_ShmKey     = (uint)shmKey   ;
//    m_ShmKey     = (uint)semKey   ;
//    m_BufferSize = videoBufferSize;
//    m_Res.width  = destWidth      ;
//    m_Res.height = destHeight     ;
// 
// 
// }

///Callback when video is stopped
// void VideoModel::stoppedReceivingEvent(int shmKey, int semKey)
// {
//    m_ShmKey = (uint)shmKey;
//    m_ShmKey = (uint)semKey;
// }

///Event callback
void VideoModel::deviceEvent()
{
   
}

///Update the buffer
void VideoModel::timedEvents()
{
   bool ok = true;
   m_Frame = m_pRenderer->video_renderer_render_to_texture(ok);
   qDebug() << "Render" << ok;
   if (ok)
      emit frameUpdated();
   else
      usleep(rand()%100000); //Be sure it can come back in sync
//    if ( !m_Attached ) {
//       //int shm_id = ShmManager::getShm(m_BufferSize, m_ShmKey);
//       //m_pBuffer  = ShmManager::attachShm(shm_id);
//       m_Attached = true;
//       //m_SetSetId = ShmManager::get_sem_set(m_SemKey);
//    }

   //int ret = ShmManager::sem_wait(m_SetSetId);
   /*if (ret != -1) {
      QByteArray array((char*)m_pBuffer,m_BufferSize);
      m_Frame.resize(0);
      m_Frame = array;
      emit frameUpdated();
   }
   else {
      usleep(rand()%100000); //Be sure it can come back in sync
   }*/
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
   m_pRenderer->shm_path = shmPath;
   m_Res.width = width;
   m_Res.height = height;
   m_pRenderer->width = width;
   m_pRenderer->height = height;
   m_pRenderer->video_renderer_start_shm();
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
   
}