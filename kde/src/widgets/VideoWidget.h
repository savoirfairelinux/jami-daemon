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
#ifndef VIDEO_WIDGET_H
#define VIDEO_WIDGET_H

#include <QtGui/QWidget>
#include <QtGui/QPainter>

#include "../lib/VideoModel.h"
class VideoRenderer;

///VideoWidget: A widget to display video from a framebuffer
class VideoWidget : public QWidget {
   Q_OBJECT
public:
   explicit VideoWidget(QWidget* parent =0, VideoRenderer* renderer = VideoModel::getInstance()->getPreviewRenderer());
private:
   QImage*        m_Image;
   VideoRenderer* m_pRenderer;
protected:
   virtual void paintEvent(QPaintEvent* event);
private slots:
   void update();
   void updateFrame();
   void stop();
public slots:
   void setRenderer(VideoRenderer* renderer);
   
};

#endif
