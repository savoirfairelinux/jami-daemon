/****************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                               *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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
class CallModelBase;
class InstantMessagingModel;

///Manager for all IM conversations
class LIB_EXPORT InstantMessagingModelManager : public QObject
{
   Q_OBJECT
public:

   //Singleton
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

   //Getter
   InstantMessagingModel* getModel(Call* call);
private:
   //Constructor
   explicit InstantMessagingModelManager();

   //Attributes
   QHash<QString,InstantMessagingModel*> m_lModels;

   //Static attributes
   static InstantMessagingModelManager* m_spInstance;
   static CallModelBase* m_spCallModel;

   
private slots:
   void newMessage(QString callId, QString from, QString message);

   
signals:
   ///Emitted when a new message is available
   void newMessagingModel(Call*,InstantMessagingModel*);
};

///Qt model for the Instant Messaging (IM) features
class LIB_EXPORT InstantMessagingModel : public QAbstractListModel
{
   Q_OBJECT
   friend class InstantMessagingModelManager;
   friend class Call;
public:
   //Role const
   static const int MESSAGE_TYPE_ROLE    = 100;
   static const int MESSAGE_FROM_ROLE    = 101;
   static const int MESSAGE_TEXT_ROLE    = 102;
   static const int MESSAGE_IMAGE_ROLE   = 103;
   static const int MESSAGE_CONTACT_ROLE = 104;

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
