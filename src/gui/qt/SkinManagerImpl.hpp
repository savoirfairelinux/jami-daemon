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


#ifndef __SKIN_MANAGER_IMPL_HPP__
#define __SKIN_MANAGER_IMPL_HPP__

class SkinManagerImpl
{
public:
  SkinManagerImpl();

 public slots:
  /**
   * This function load a given skin. If the 
   * skin is invalid, nothing is done.
   */
  loadSkin(QString skin);

  /**
   * This function load the default skin. If the 
   * skin is invalid, nothing is done.
   */
  loadDefaultSkin();

  void update();

private:
  QSettings mSettings;
  QString mDefaultSkin;
  Skin mCurrent;
};

#endif

