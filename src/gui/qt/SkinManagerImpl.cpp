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
#include "DebugOutput.hpp"
#include "SkinManagerImpl.hpp"

SkinManagerImpl::SkinManagerImpl() 
  : mApp(NULL)
  , mHunter(SKINDIR)
{
  mSettings.setPath("savoirfairelinux.com", PROGNAME);
  mSettings.beginGroup("/" PROGNAME);
  mPaths = mSettings.readListEntry("SkinPaths");
}

void
SkinManagerImpl::setApplication(QApplication *app)
{
  mApp = app;
}

void
SkinManagerImpl::load()
{
  bool ok;
  load(mSettings.readEntry("Skin", "metal", &ok));
  if(!ok) {
    mSettings.writeEntry("Skin", "metal");
  }
}

void 
SkinManagerImpl::save()
{
  mSettings.writeEntry("Skin", mSkin);
  mSettings.writeEntry("SkinPaths", mPaths);
}

void
SkinManagerImpl::load(const QString &skin)
{
  mSkin = skin;
  if(mApp) {
    taxidermy::Taxidermist taxidermist = mHunter.getTaxidermist(skin);
    taxidermist.update(mApp);
  }
}

QStringList
SkinManagerImpl::getSkins()
{
  return mHunter.getSkinNames();
}

void
SkinManagerImpl::addPath(const QString &path) 
{
  mPaths.push_back(path);
}

QStringList
SkinManagerImpl::getPaths()
{
  return mPaths;
}
