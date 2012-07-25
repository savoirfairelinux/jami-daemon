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
#include "VideoDock.h"

#include <QtGui/QSpacerItem>
#include <QtGui/QGridLayout>

#include <KLocale>

#include "VideoWidget.h"

///Constructor
VideoDock::VideoDock(QWidget* parent) : QDockWidget(parent)
{
   setWindowTitle(i18nc("Video conversation","Video"));
   QWidget* wdg = new QWidget(this);
   m_pVideoWidet = new VideoWidget(this);
   auto l = new QGridLayout(wdg);
   l->addWidget(m_pVideoWidet,1,1);
   l->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding),0,0);
   l->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding),0,1);
   l->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding),2,0);
   l->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding),0,2);
   setWidget(wdg);
}

///Set current renderer
void VideoDock::setRenderer(VideoRenderer* r)
{
   m_pVideoWidet->setRenderer(r);
}
