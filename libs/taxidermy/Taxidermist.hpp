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

#ifndef __TAXIDERMY_TAXIDERMIST_HPP__
#define __TAXIDERMY_TAXIDERMIST_HPP__

#include <qmap.h>
#include <qstring.h>
#include <qwidget.h>

namespace taxidermy {

  class WidgetBuilder;

  class Taxidermist
  {
  private:
    QMap< QString, WidgetBuilder * > mBuilders;
    QString mSkinName;

  public:
    Taxidermist(const QString &skinName);

    QString name() 
    {return mSkinName;}
    
    /**
     * This function will set the skin to the widget.
     * It will use the widget's name to find what to
     * specify for the skin.
     */
    void skin(QWidget *widget);

    /**
     * This function will add a widget builder for a specific
     * widget. This widget is identified by his name.
     */
    void add(const QString &name, WidgetBuilder *builder);

    /**
     * This function will load the current skin, by
     * retreiving each QWidget skinnable and updating it.
     *
     * Note: if you have an another skin currently used, you
     * need to unload it. 
     */
    void update(QApplication *app);
  };

};

#endif 
