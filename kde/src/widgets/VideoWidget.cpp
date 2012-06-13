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
#include "VideoWidget.h"
#include <KDebug>

VideoWidget::VideoWidget(QWidget* parent) : QWidget(parent),m_Image(NULL) {
   setMinimumSize(200,200);
   connect(VideoModel::getInstance(),SIGNAL(frameUpdated()),this,SLOT(repaint2()));
}

void VideoWidget::update() {
   QPainter painter(this);
   painter.drawImage(QRect(0,0,width(),height()),*(m_Image));
   painter.end();
}

void VideoWidget::paintEvent(QPaintEvent* event)
{
   Q_UNUSED(event)
   if (VideoModel::getInstance()->isPreviewing()) {
      update();
   }
}

void VideoWidget::repaint2()
{
   QSize size(VideoModel::getInstance()->getActiveResolution().width, VideoModel::getInstance()->getActiveResolution().height);
   if (size != minimumSize())
      setMinimumSize(size);
   //if (m_Image)
   //   delete m_Image;
   m_Image = new QImage(size,QImage::Format_ARGB32);
   m_Image->loadFromData(VideoModel::getInstance()->getCurrentFrame(),"BMP");
   repaint();
}