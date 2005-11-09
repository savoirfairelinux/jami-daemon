/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *                                                                              
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                              
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <qwidget.h>

#include "Skin.hpp"

Skin::Skin(QSettings settings)
{
  load(settings);
}

void
Skin::load(QSettings settings)
{
  QStringList keys = settings.subkeyList("/");
  for(QStringList::Iterator it = keys.begin(); it != keys.end(); ++it) {
    settings.beginGroup("/" + *it);
    SkinElement elem;

    bool pixname, x, y;
    elem.pixname = settings.readEntry("/pixname", QString::null, okay);
    elem.x = settings.readNumEntry("/x", 0, &okay);
    elem.y = settings.readNumEntry("/y", 0, &okay);
    if(!pixname || !x || !y) {
      DebugOutput::instance() << QObject::tr("The Skin entry %1 isn't complete")
	.arg(*it);
    }
    else {
      mSettings.insert(std::make_pair(*it, elem));
    }
  }
}

template< typename T >
void
Skin::update(T *widget)
{
  SettingsType::iterator pos = mSettings.find(widget->name());
  if(pos != mSettings.end()) {
    w->setPixmap(pos->second.pixmap);
    w->move(pos->second.x, pos->second.y);
  }
}

