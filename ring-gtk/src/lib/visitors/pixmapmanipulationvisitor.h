/****************************************************************************
 *   Copyright (C) 2013-2014 by Savoir-Faire Linux                         ***
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
#ifndef PIXMAPMANIPULATIONVISITOR_H
#define PIXMAPMANIPULATIONVISITOR_H
#include "../typedefs.h"

//Qt
#include <QtCore/QVariant>
#include <QtCore/QModelIndex>

//SFLPhone
class Contact    ;
class PhoneNumber;
class Call       ;

/**
 * Different clients can have multiple way of displaying images. Some may
 * add borders, other add corner radius (see Ubuntu-SDK HIG). This
 * abstract class define many operations that can be defined by each clients.
 *
 * Most methods return QVariants as this library doesn't link against QtGui
 *
 * This interface is not frozen, more methods may be added later. To implement,
 * just create an object somewhere, be sure to call PixmapManipulationVisitor()
 */
class LIB_EXPORT PixmapManipulationVisitor {
public:
   PixmapManipulationVisitor();
   virtual ~PixmapManipulationVisitor() {}
   virtual QVariant contactPhoto(Contact* c, const QSize& size, bool displayPresence = true);
   virtual QVariant callPhoto(Call* c, const QSize& size, bool displayPresence = true);
   virtual QByteArray toByteArray(const QVariant pxm);
   virtual QVariant profilePhoto(const QByteArray& data);
   virtual QVariant callPhoto(const PhoneNumber* n, const QSize& size, bool displayPresence = true);
   virtual QVariant numberCategoryIcon(const QVariant p, const QSize& size, bool displayPresence = false, bool isPresent = false);
   virtual QVariant serurityIssueIcon(const QModelIndex& index);

   //Singleton
   static PixmapManipulationVisitor* instance();
protected:
   static PixmapManipulationVisitor* m_spInstance;
};

#endif //PIXMAPMANIPULATIONVISITOR_H
