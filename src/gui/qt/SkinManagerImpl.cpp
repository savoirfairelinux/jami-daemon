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

#include "globals.h"
#include "Skin.hpp"
#include "SkinManagerImpl.hpp"

SkinManagerImpl::SkinManagerImpl(QSettings settings)
{
  load(settings);
}

void
SkinManagerImpl::load(QSettings settings)
{
  mSettings = settings;
  mSettings.beginGroup("/Skins");

  QStringList skins = mSettings.subkeyList("/");
  for(QStringList::Iterator it = skins.begin(); it != skins.end(); ++it) {
    mSettings.beginGroup("/" + *it);
    mSkins.insert(std::make_pair(*it, Skin(mSettings)));
    mSettings.endGroup():
  }

  mDefaultSkinName = mSettings.readEntry("/default");

  loadDefaultSkin();
}

void
SkinManagerImpl::loadSkin(QString skin)
{
  SkinsType::iterator pos = mSkins.find(skin);
  if(pos != mSkins.end()) {
    mCurrent = pos->second();
  }
}

void
SkinManagerImpl::loadDefaultSkin()
{
  loadSkin(mDefaultSkinName);
}

void
SkinManagerImpl::update()
{
  QWidgetList *list = QApplication::allWidgets();
  QWidgetListIt it(*list);         // iterate over the widgets
  QWidget * w;
  while ((w=it.current()) != 0) {  // for each widget...
    ++it;
    mCurrent->update(w);
  }
  delete list;      
}
