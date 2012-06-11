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

#include "dlgvideo.h"
#include "../lib/VideoDevice.h"
#include "../lib/VideoCodec.h"

DlgVideo::DlgVideo(QWidget *parent)
 : QWidget(parent),m_pDevice(NULL)
{
   setupUi(this);
   
   QList<VideoDevice*> devices =  VideoDevice::getDeviceList();
   foreach(VideoDevice* dev,devices) {
      m_pDeviceCB->addItem(dev->getDeviceId());
   }

   connect(m_pDeviceCB    ,SIGNAL(currentIndexChanged(QString)),this,SLOT(loadDevice(QString)     ));
   connect(m_pChannelCB   ,SIGNAL(currentIndexChanged(QString)),this,SLOT(loadResolution(QString) ));
   connect(m_pResolutionCB,SIGNAL(currentIndexChanged(QString)),this,SLOT(loadRate(QString)       ));

   QList<VideoCodec*> codecs = VideoCodec::getCodecList();
   foreach(VideoCodec* codec,codecs) {
      m_pCodecCB->addItem(codec->getName());
   }

   m_pConfGB->setEnabled(devices.size());

   if (devices.size())
      loadDevice(devices[0]->getDeviceId());
}


DlgVideo::~DlgVideo()
{
   
}

void DlgVideo::loadDevice(QString device) {
   m_pDevice = VideoDevice::getDevice(device);
   if (m_pDevice) {
      m_pChannelCB->clear();
      foreach(VideoChannel channel,m_pDevice->getChannelList()) {
         m_pChannelCB->addItem(channel);
      }
   }
}

void DlgVideo::loadResolution(QString channel)
{
   m_pResolutionCB->clear();
   foreach(Resolution res,m_pDevice->getResolutionList(channel)) {
      m_pResolutionCB->addItem(res.toString());
   }
}

void DlgVideo::loadRate(QString resolution)
{
   m_pRateCB->clear();
   foreach(QString rate,m_pDevice->getRateList(m_pChannelCB->currentText(),resolution)) {
      m_pRateCB->addItem(rate);
   }
}