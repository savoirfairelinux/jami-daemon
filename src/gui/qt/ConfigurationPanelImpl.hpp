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

#ifndef __CONFIGURATION_PANEL_IMPL_HPP__
#define __CONFIGURATION_PANEL_IMPL_HPP__

#include <list>
#include <map>
#include <qlayout.h>
#include <qdialog.h>

struct ConfigEntry
{
public:
  ConfigEntry(QString s,
	      QString n,
	      QString t,
	      QString d,
	      QString v) 
  {
    section = s;
    name = n;
    type = t;
    def = d;
    value = v;
  }

  QString section;
  QString name;
  QString type;
  QString def;
  QString value;
};

class ConfigurationPanelImpl : public QDialog
{
  Q_OBJECT

public:
  ConfigurationPanelImpl(QWidget *parent = NULL);

public slots:
  void add(const ConfigEntry &entry);
  void generate();

private:
  std::map< QString, std::list< ConfigEntry > > mEntries;
  QVBoxLayout *mLayout;
};


#endif 
