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
#include "video_interface_singleton.h"

///Constructor
VideoModel::VideoModel():m_BufferSize(0),m_ShmKey(0),m_SemKey(0),m_Res(0,0)
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   connect(&interface,SIGNAL(receivingEvent(int,int,int,int,int)),this,SLOT(receivingEvent(int,int,int,int,int)));
   connect(&interface,SIGNAL(deviceEvent()),this,SLOT(deviceEvent()));
   connect(&interface,SIGNAL(stoppedReceivingEvent(int,int)),this,SLOT(stoppedReceivingEvent(int,int)));
}

///Stop video preview
void VideoModel::stopPreview()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.stopPreview();
}

///Start video preview
void VideoModel::startPreview()
{
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   interface.startPreview();
}

///@todo Set the video buffer size
void VideoModel::setBufferSize(uint size)
{
   m_BufferSize = size;
}

///Event callback
void VideoModel::receivingEvent(int shmKey, int semKey, int videoBufferSize, int destWidth, int destHeight)
{
   m_ShmKey     = (uint)shmKey;
   m_ShmKey     = (uint)semKey;
   m_BufferSize = videoBufferSize;
   m_Res.width  = destWidth;
   m_Res.height = destHeight;
}

///Callback when video is stopped
void VideoModel::stoppedReceivingEvent(int shmKey, int semKey)
{
   m_ShmKey     = (uint)shmKey;
   m_ShmKey     = (uint)semKey;
}

///Event callback
void VideoModel::deviceEvent()
{
   
}