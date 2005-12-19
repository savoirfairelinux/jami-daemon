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

#ifndef TAXIDERMY_WIDGET_BUILDER_FACTORY_IMPL_HPP
#define TAXIDERMY_WIDGET_BUILDER_FACTORY_IMPL_HPP

#include <qmap.h>

#include "WidgetBuilderCreator.hpp"

namespace taxidermy
{
  class WidgetBuilderFactoryImpl
  {
  private:
    QMap< QString, WidgetBuilderCreatorBase * > mBuilders;

  public:
    WidgetBuilderFactoryImpl();

    template< typename T >
    void add(const QString &objectType);

    WidgetBuilder *create(const QString &objectType);
  };
};

#include "WidgetBuilderFactoryImpl.inl"

#endif
