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
#include "typedefs.h"
#include <QtCore/QDebug>

enum MessageRole {
   INCOMMING_IM,
   OUTGOING_IM,
   MIME_TRANSFER,
};

class Call;
class CallModelBase;
class InstantMessagingModel;

class LIB_EXPORT InstantMessagingModelManager : public QObject
{
   Q_OBJECT
public:
   static InstantMessagingModelManager* getInstance() {
      if (!m_spInstance) {
         m_spInstance = new InstantMessagingModelManager();
      }
      return m_spInstance;
   }
   static void init(CallModelBase* model) {
      m_spCallModel = model;
      getInstance();
   }
   InstantMessagingModel* getModel(Call* call);
private:
   InstantMessagingModelManager();
   static InstantMessagingModelManager* m_spInstance;
   static CallModelBase* m_spCallModel;
   QHash<QString,InstantMessagingModel*> m_lModels;
private slots:
   void newMessage(QString callId, QString from, QString message);

signals:
   void newMessagingModel(Call*,InstantMessagingModel*);
};

class LIB_EXPORT InstantMessagingModel : public QAbstractListModel
{
   Q_OBJECT
   friend class InstantMessagingModelManager;
   friend class Call;
public:
   static const int MESSAGE_TYPE_ROLE    = 100;
   static const int MESSAGE_FROM_ROLE    = 101;
   static const int MESSAGE_TEXT_ROLE    = 102;
   static const int MESSAGE_IMAGE_ROLE   = 103;
   static const int MESSAGE_CONTACT_ROLE = 104;

   InstantMessagingModel(Call* call, QObject* parent = nullptr);
   QVariant      data     ( const QModelIndex& index, int role = Qt::DisplayRole     ) const;
   int           rowCount ( const QModelIndex& parent = QModelIndex()                ) const;
   Qt::ItemFlags flags    ( const QModelIndex& index                                 ) const;
   virtual bool  setData  ( const QModelIndex& index, const QVariant &value, int role)      ;

private:
   void addIncommingMessage(QString from, QString message);
   void addOutgoingMessage(QString message);
   
   struct InternalIM {
      QString from;
      QString message;
   };
   QList<InternalIM> m_lMessages;
   QHash<QModelIndex,QVariant> m_lImages;
   Call* m_pCall;

};

#endif