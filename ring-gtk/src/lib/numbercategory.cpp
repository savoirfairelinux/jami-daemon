/****************************************************************************
 *   Copyright (C) 2013-2014 by Savoir-Faire Linux                          *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/
#include "numbercategory.h"

#include <QtCore/QSize>

#include "visitors/pixmapmanipulationvisitor.h"


NumberCategory::NumberCategory(QObject* parent, const QString& name) : QObject(parent),m_Name(name)
{

}

QVariant NumberCategory::icon(bool isTracked, bool isPresent) const
{
   return PixmapManipulationVisitor::instance()->numberCategoryIcon(m_pIcon,QSize(),isTracked,isPresent);
}

QString  NumberCategory::name() const
{
   return m_Name;
}

void NumberCategory::setIcon(QVariant pixmap)
{
   m_pIcon = pixmap;
}

void NumberCategory::setName(const QString& name)
{
   m_Name = name;
}
