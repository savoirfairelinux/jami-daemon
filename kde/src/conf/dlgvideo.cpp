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

#include "dlgvideo.h"
#include "../lib/VideoDevice.h"
#include "../lib/VideoCodecModel.h"
#include "../lib/VideoModel.h"
#include <KDebug>

DlgVideo::DlgVideo(QWidget *parent)
 : QWidget(parent),m_pDevice(nullptr)
{
   setupUi(this);
   
   const QList<VideoDevice*> devices =  VideoDevice::getDeviceList();
   foreach(VideoDevice* dev,devices) {
      m_pDeviceCB->addItem(dev->getDeviceId());
   }

   connect(m_pDeviceCB    ,SIGNAL(currentIndexChanged(QString)),this,SLOT(loadDevice(QString))    );
   connect(m_pChannelCB   ,SIGNAL(currentIndexChanged(QString)),this,SLOT(loadResolution(QString)));
   connect(m_pResolutionCB,SIGNAL(currentIndexChanged(QString)),this,SLOT(loadRate(QString))      );
   connect(m_pRateCB      ,SIGNAL(currentIndexChanged(QString)),this,SLOT(changeRate(QString))    );
   connect(m_pPreviewPB   ,SIGNAL(clicked())                   ,this,SLOT(startStopPreview())     );


   m_pConfGB->setEnabled(devices.size());

   if (devices.size())
      loadDevice(devices[0]->getDeviceId());

   if (VideoModel::getInstance()->isPreviewing()) {
      m_pPreviewPB->setText(i18n("Stop preview"));
   }
}


DlgVideo::~DlgVideo()
{
   VideoModel::getInstance()->stopPreview();
}

void DlgVideo::loadDevice(QString device) {
   m_pDevice = VideoDevice::getDevice(device);
   QString curChan = m_pDevice->getChannel();
   if (m_pDevice) {
      m_pChannelCB->clear();
      foreach(const VideoChannel& channel,m_pDevice->getChannelList()) {
         m_pChannelCB->addItem(channel);
         if (channel == curChan)
            m_pChannelCB->setCurrentIndex(m_pChannelCB->count()-1);
      }
   }
}

void DlgVideo::loadResolution(QString channel)
{
   Resolution current = m_pDevice->getResolution();
   m_pResolutionCB->clear();
   foreach(const Resolution& res,m_pDevice->getResolutionList(channel)) {
      m_pResolutionCB->addItem(res.toString());
      if (current == res) {
         m_pResolutionCB->setCurrentIndex(m_pResolutionCB->count()-1);
      }
   }
   m_pDevice->setChannel(channel);
}

void DlgVideo::loadRate(QString resolution)
{
   m_pRateCB->clear();
   QString rate = m_pDevice->getRate();
   foreach(const QString& r,m_pDevice->getRateList(m_pChannelCB->currentText(),resolution)) {
      m_pRateCB->addItem(r);
      if (r == rate)
         m_pRateCB->setCurrentIndex(m_pRateCB->count()-1);
   }
   m_pDevice->setResolution(resolution);
}

void DlgVideo::changeRate(QString rate)
{
   m_pDevice->setRate(rate);
}

void DlgVideo::startStopPreview()
{
   if (VideoModel::getInstance()->isPreviewing()) {
      m_pPreviewPB->setText(i18n("Start preview"));
      VideoModel::getInstance()->stopPreview();
   }
   else {
      m_pPreviewPB->setText(i18n("Stop preview"));
      VideoModel::getInstance()->startPreview();
   }
}
