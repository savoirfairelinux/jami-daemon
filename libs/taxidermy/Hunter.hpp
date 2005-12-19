/*
 * Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 * Author: Jean-Philippe Barrette-LaPierre
 *            (jean-philippe.barrette-lapierre@savoirfairelinux.com)
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dpkg; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __TAXIDERMY_HUNTER_HPP__
#define __TAXIDERMY_HUNTER_HPP__

#include <qdir.h>
#include <qfile.h>
#include <qstring.h>
#include <qstringlist.h>

#include "Taxidermist.hpp"
#include "config.h"

namespace taxidermy
{
  
  /**
   * This class is responsible of finding skins directories.
   * It is also responsible of reading the settings.
   */
  class Hunter 
  {
  private:
    QDir mSkinsDirectory;

  public:
    Hunter();
    Hunter(const QString &directory);
    Hunter(const QDir &dir);

    QStringList getSkinNames();
    QString getSkinFile(const QString &skinName);
    Taxidermist getTaxidermist(const QString &skinName);
    void load(Taxidermist *skin);
    //static void load(QFile file, Taxidermist *skin);
  };
};

#endif 
