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
#ifndef NUMBERCATEGORY_H
#define NUMBERCATEGORY_H

#include <QtCore/QObject>

#include "typedefs.h"

class QPixmap;

/**
 * This class represent a PhoneNumber category. Categories usually
 * come from the contact provider, but can be added dynamically too
 */
class LIB_EXPORT NumberCategory : public QObject {
   Q_OBJECT
public:
   friend class NumberCategoryModel;
   virtual ~NumberCategory(){}

   //Getter
   QVariant icon(bool isTracked = false, bool isPresent = false) const;
   QString  name() const;

   //Setter
   void setIcon(QPixmap*       pixmap );
   void setName(const QString& name   );

private:
   NumberCategory(QObject* parent, const QString& name);

   //Attributes
   QString m_Name;
   QPixmap* m_pIcon;
};

#endif //NUMBERCATEGORY_H
