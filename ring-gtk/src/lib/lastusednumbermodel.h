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
#ifndef LASTUSEDNUMBERMODEL_H
#define LASTUSEDNUMBERMODEL_H

#include "typedefs.h"

#include <QtCore/QAbstractListModel>

struct ChainedPhoneNumber;
class Call;
class PhoneNumber;

class LIB_EXPORT LastUsedNumberModel : public QAbstractListModel {
   Q_OBJECT

public:
   //Singleton
   static LastUsedNumberModel* instance();

   //Model functions
   QVariant      data     ( const QModelIndex& index, int role = Qt::DisplayRole     ) const;
   int           rowCount ( const QModelIndex& parent = QModelIndex()                ) const;
   Qt::ItemFlags flags    ( const QModelIndex& index                                 ) const;
   virtual bool  setData  ( const QModelIndex& index, const QVariant &value, int role)      ;

   //Mutator
   void addCall(Call* call);
private:
   //Const
   static const int MAX_ITEM = 15;

   //Private constructor
   LastUsedNumberModel();

   //Attributes
   ChainedPhoneNumber* m_pFirstNode;
   QHash<PhoneNumber*,ChainedPhoneNumber*> m_hNumbers;
   bool m_IsValid;
   ChainedPhoneNumber* m_lLastNumbers[MAX_ITEM];

   //Static attributes
   static LastUsedNumberModel* m_spInstance;
};

#endif
