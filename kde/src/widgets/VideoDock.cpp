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
#include "VideoDock.h"

#include <QtGui/QSpacerItem>
#include <QtGui/QGridLayout>

#include <KLocale>

#include "VideoWidget.h"

///Constructor
VideoDock::VideoDock(QWidget* parent) : QDockWidget(parent)
{
   setWindowTitle(i18n("Video"));
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

void VideoDock::setRenderer(VideoRenderer* r)
{
   m_pVideoWidet->setRenderer(r);
}