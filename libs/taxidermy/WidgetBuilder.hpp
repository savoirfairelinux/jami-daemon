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

#include <qmap.h>
#include <qobject.h>
#include <qsettings.h>
#include <qstring.h>
#include <qwidget.h>

namespace taxidermy {
  /**
   * This class is responsible of loading a skin for a
   * type of widget from a QSettings, and then be able to
   * set the values loaded to a widget.
   */
  class WidgetBuilder : public QObject
  {
    Q_OBJECT			

  private:
    QString mObjectType;

  private:
    WidgetBuilder();

  public:
    WidgetBuilder(const QString &objectType);
    virtual ~WidgetBuilder() {}

    QString getObjectType();
    
    virtual void load(const QMap< QString, QString > &entries) = 0;
    virtual void update() = 0;
    virtual void update(QWidget *widget) = 0;
  };
};
