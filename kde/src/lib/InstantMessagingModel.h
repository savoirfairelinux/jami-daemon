/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/

#ifndef IM_MODEL_H
#define IM_MODEL_H

#include <QtCore/QAbstractListModel>
#include <QtCore/QModelIndex>

enum MessageRole {
   INCOMMING_IM,
   OUTGOING_IM,
   MIME_TRANSFER,
};

class InstantMessagingModel : public QAbstractListModel
{
   Q_OBJECT
public:
   static const int MESSAGE_TYPE_ROLE    = 100;
   static const int MESSAGE_FROM_ROLE    = 101;
   static const int MESSAGE_TEXT_ROLE    = 102;
   static const int MESSAGE_CONTACT_ROLE = 103;

   InstantMessagingModel(QObject* parent);
   QVariant      data     ( const QModelIndex& index, int role = Qt::DisplayRole     ) const;
   int           rowCount ( const QModelIndex& parent = QModelIndex()                ) const;
   Qt::ItemFlags flags    ( const QModelIndex& index                                 ) const;
   virtual bool  setData  ( const QModelIndex& index, const QVariant &value, int role)      ;

private:
   struct InternalIM {
      QString from;
      QString message;
   };
   QList<InternalIM> m_lMessages;
};

#endif