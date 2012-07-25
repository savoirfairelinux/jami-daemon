/***************************************************************************
 *   Copyright (C) 2011 by Savoir-Faire Linux                              *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#ifndef VIDEO_DOCK_H
#define VIDEO_DOCK_H

#include <QtGui/QDockWidget>

//Qt
class QSpacerItem;

//SFLPhone
class VideoWidget;
class VideoRenderer;

///VideoDock: A dock hosting a VideoWidget or AcceleratedVideoWidget
class VideoDock : public QDockWidget {
   Q_OBJECT
public:
   VideoDock(QWidget* parent =0 );
   void setRenderer(VideoRenderer* r);
   
private:
   VideoWidget* m_pVideoWidet;
   
private slots:
   
};

#endif
