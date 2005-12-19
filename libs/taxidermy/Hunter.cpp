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

#include <iostream>
#include <stdexcept>

#include "Hunter.hpp"
#include "WidgetBuilder.hpp"
#include "WidgetBuilderFactory.hpp"
#include "config.h"


#define DEFAULT_DIRECTORY "skins/"

#define fill_config_str(name, value) \
  (_config.addConfigTreeItem(section, Conf::ConfigTreeItem(std::string(name), std::string(value), type_str)))
#define fill_config_int(name, value) \
  (_config.addConfigTreeItem(section, Conf::ConfigTreeItem(std::string(name), std::string(value), type_int)))


taxidermy::Hunter::Hunter()
  : mSkinsDirectory(DEFAULT_DIRECTORY)
{}

taxidermy::Hunter::Hunter(const QString &directory)
  : mSkinsDirectory(directory)
{}

taxidermy::Hunter::Hunter(const QDir &directory)
  : mSkinsDirectory(directory)
{}

QStringList
taxidermy::Hunter::getSkinNames() 
{
  QStringList skins;
  mSkinsDirectory.setNameFilter("*.ini");
  QStringList list = mSkinsDirectory.entryList(QDir::Files);
  for(QStringList::Iterator it = list.begin(); it != list.end(); ++it) {
    QString skin(*it);
    skin = skin.left(skin.length() - 4);
    skins.push_back(skin);
  }

  return skins;
}

QString
taxidermy::Hunter::getSkinFile(const QString &name)
{
  return mSkinsDirectory.absPath() + "/" + name + ".ini";
}

taxidermy::Taxidermist
taxidermy::Hunter::getTaxidermist(const QString &skinName)
{
  taxidermy::Taxidermist skin(skinName);
  load(&skin);
  
  return skin;
}

// void
// taxidermy::Hunter::load(taxidermy::Taxidermist *skin) 
// {
//   load(mSkinsDirectory, skin);
// }

void
taxidermy::Hunter::load(taxidermy::Taxidermist *skin) 
{
  Conf::ConfigTree config;
  config.populateFromFile(getSkinFile(skin->name()));
  Conf::TokenList tree = config.getSectionNames();
  for(Conf::TokenList::iterator it = tree.begin(); it != tree.end(); it++) {
    Conf::ValuesMap values = config.getSection(*it);
    Conf::ValuesMap::iterator type = values.find("type");

    if(type != values.end()) {
      QMap< QString, QString > v;
      WidgetBuilder *builder = 
	taxidermy::WidgetBuilderFactory::instance().create(type->second);
      if(builder != NULL) {
	for(Conf::ValuesMap::iterator pos = values.begin();
	    pos != values.end();
	    pos++) {
	  v.insert(pos->first, pos->second);
	}
	builder->load(v);
	skin->add(*it, builder);
      }
    }
  }
}
