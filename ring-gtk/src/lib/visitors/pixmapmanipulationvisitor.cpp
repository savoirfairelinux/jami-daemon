/****************************************************************************
 *   Copyright (C) 2013-2014 by Savoir-Faire Linux                           *
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
#include "pixmapmanipulationvisitor.h"

#include <QtCore/QSize>

PixmapManipulationVisitor* PixmapManipulationVisitor::m_spInstance = new PixmapManipulationVisitor();

PixmapManipulationVisitor::PixmapManipulationVisitor() {
   m_spInstance = this;
}

QVariant PixmapManipulationVisitor::contactPhoto(Contact* c, const QSize& size, bool displayPresence)
{
   Q_UNUSED(c)
   Q_UNUSED(size)
   Q_UNUSED(displayPresence)
   return QVariant();
}

QByteArray PixmapManipulationVisitor::toByteArray(const QPixmap* pxm)
{
   Q_UNUSED(pxm)
   return *(new QByteArray());
}

QVariant PixmapManipulationVisitor::numberCategoryIcon(const QPixmap* p, const QSize& size, bool displayPresence, bool isPresent)
{
   Q_UNUSED(p)
   Q_UNUSED(size)
   Q_UNUSED(displayPresence)
   Q_UNUSED(isPresent)
   return QVariant();
}

QVariant PixmapManipulationVisitor::callPhoto(Call* c, const QSize& size, bool displayPresence)
{
   Q_UNUSED(c)
   Q_UNUSED(size)
   Q_UNUSED(displayPresence)
   return QVariant();
}

QVariant PixmapManipulationVisitor::callPhoto(const PhoneNumber* c, const QSize& size, bool displayPresence)
{
   Q_UNUSED(c)
   Q_UNUSED(size)
   Q_UNUSED(displayPresence)
   return QVariant();
}

PixmapManipulationVisitor* PixmapManipulationVisitor::instance()
{
   return m_spInstance;
}

QVariant PixmapManipulationVisitor::serurityIssueIcon(const QModelIndex& index)
{
   Q_UNUSED(index)
   return QVariant();
}
