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

#include <stdexcept>
#include <qapplication.h>
#include <qwidgetlist.h>

#include "Hunter.hpp"
#include "Taxidermist.hpp"
#include "WidgetBuilder.hpp"

taxidermy::Taxidermist::Taxidermist(const QString &skinName) 
{
  mSkinName = skinName;
}

void
taxidermy::Taxidermist::add(const QString &name, WidgetBuilder *builder)
{
  mBuilders.insert(name, builder, true);
}

void
taxidermy::Taxidermist::skin(QWidget *widget)
{
  QMap< QString, WidgetBuilder * >::Iterator pos = mBuilders.find(widget->name());
  if(pos != mBuilders.end()) {
    (*pos)->update(widget);
  }
}

void
taxidermy::Taxidermist::update(QApplication *app) 
{
  QWidget *mainWidget = app->mainWidget();
  if(mainWidget) {
    for(QMap< QString, WidgetBuilder * >::Iterator pos = mBuilders.begin();
	pos != mBuilders.end();
	pos++) {
      QObject *w = mainWidget->child(pos.key());
      if(w && w->isWidgetType()) {
	pos.data()->update((QWidget *)w);
      }
    }
  }
}


