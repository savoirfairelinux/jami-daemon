/****************************************************************************
 *   Copyright (C) 2012-2014 by Savoir-Faire Linux                          *
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
#ifndef INSTANTMESSAGINGMODELMANAGER_H
#define INSTANTMESSAGINGMODELMANAGER_H

#include <QtCore/QAbstractListModel>
#include <QtCore/QModelIndex>
#include "typedefs.h"
#include <QtCore/QDebug>

///Represent the direction a message is taking
enum MessageRole {
   INCOMMING_IM ,
   OUTGOING_IM  ,
   MIME_TRANSFER,
};

//SFLPhone
class Call;
class InstantMessagingModel;

///Manager for all IM conversations
class LIB_EXPORT InstantMessagingModelManager : public QObject
{
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
public:

   //Singleton
   static InstantMessagingModelManager* instance();
   static void init();

   //Getter
   InstantMessagingModel* getModel(Call* call);
private:
   //Constructor
   explicit InstantMessagingModelManager();

   //Attributes
   QHash<QString,InstantMessagingModel*> m_lModels;

   //Static attributes
   static InstantMessagingModelManager* m_spInstance;


private Q_SLOTS:
   void newMessage(QString callId, QString from, QString message);


Q_SIGNALS:
   ///Emitted when a new message is available
   void newMessagingModel(Call*,InstantMessagingModel*);
};

///Qt model for the Instant Messaging (IM) features
class LIB_EXPORT InstantMessagingModel : public QAbstractListModel
{
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
   friend class InstantMessagingModelManager;
   friend class Call;
public:
   //Role const
   enum Role {
      TYPE    = 100,
      FROM    = 101,
      TEXT    = 102,
      IMAGE   = 103,
      CONTACT = 104,
   };

   //Constructor
   explicit InstantMessagingModel(Call* call, QObject* parent = nullptr);

   //Abstract model function
   QVariant      data     ( const QModelIndex& index, int role = Qt::DisplayRole     ) const;
   int           rowCount ( const QModelIndex& parent = QModelIndex()                ) const;
   Qt::ItemFlags flags    ( const QModelIndex& index                                 ) const;
   virtual bool  setData  ( const QModelIndex& index, const QVariant &value, int role)      ;

private:
   //Private members
   void addIncommingMessage(QString from, QString message);
   void addOutgoingMessage(QString message);

   //Interal struct
   struct InternalIM {
      QString from;
      QString message;
   };

   //Attributes
   QList<InternalIM>           m_lMessages;
   QHash<QModelIndex,QVariant> m_lImages  ;
   Call*                       m_pCall    ;
};
#endif
