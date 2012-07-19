/************************************************************************************
 *   Copyright (C) 2012 by Savoir-Faire Linux                                       *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
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
#include "InstantMessagingModel.h"
#include "CallModel.h"
#include "callmanager_interface_singleton.h"
#include "Call.h"

InstantMessagingModelManager* InstantMessagingModelManager::m_spInstance  = nullptr;
CallModelBase*                InstantMessagingModelManager::m_spCallModel = nullptr;

void InstantMessagingModelManager::newMessage(QString callId, QString from, QString message)
{
   if (!m_lModels[callId] && m_spCallModel) {
      Call* call = m_spCallModel->getCall(callId);
      if (call) {
         qDebug() << "Creating messaging model for call" << callId;
         m_lModels[callId] = new InstantMessagingModel(call);
         emit newMessagingModel(call,m_lModels[callId]);
         m_lModels[callId]->addIncommingMessage(from,message);
      }
   }
   else if (m_lModels[callId]) {
      m_lModels[callId]->addIncommingMessage(from,message);
   }
}

InstantMessagingModelManager::InstantMessagingModelManager() : QObject(0)
{
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   connect(&callManager, SIGNAL(incomingMessage(const QString &, const QString &, const QString &)), this, SLOT(newMessage(const QString &, const QString &, const QString &)));
}

InstantMessagingModel* InstantMessagingModelManager::getModel(Call* call) {
   QString key = call->isConference()?call->getConfId():call->getCallId();
   if (!m_lModels[key]) {
      m_lModels[key] = new InstantMessagingModel(call);
      emit newMessagingModel(call,m_lModels[key]);
   }
   return m_lModels[key];
}

InstantMessagingModel::InstantMessagingModel(Call* call, QObject* parent) : QAbstractListModel(parent),m_pCall(call)
{
   //QStringList callList = callManager.getCallList();
}

QVariant InstantMessagingModel::data( const QModelIndex& index, int role) const
{
   if(index.column() == 0 && role == Qt::DisplayRole)
      return QVariant(m_lMessages[index.row()].message);
   else if (index.column() == 0 && role == MESSAGE_TYPE_ROLE    )
      return QVariant(m_lMessages[index.row()].message);
   else if (index.column() == 0 && role == MESSAGE_FROM_ROLE    )
      return QVariant(m_lMessages[index.row()].from);
   else if (index.column() == 0 && role == MESSAGE_TEXT_ROLE    )
      return INCOMMING_IM;
   else if (index.column() == 0 && role == MESSAGE_CONTACT_ROLE  && m_pCall->getContact())
      return QVariant();
   else if (index.column() == 0 && role == MESSAGE_IMAGE_ROLE   ) {
      if (m_lImages.find(index) != m_lImages.end())
         return m_lImages[index];
      Contact* c =m_pCall->getContact();
      if (c && c->getPhoto()) {
         return QVariant::fromValue<void*>((void*)c->getPhoto());
      }
      return QVariant();
   }
   return QVariant();
}

int InstantMessagingModel::rowCount(const QModelIndex& parent) const
{
   Q_UNUSED(parent)
   return m_lMessages.size();
}

Qt::ItemFlags InstantMessagingModel::flags(const QModelIndex& index) const
{
   Q_UNUSED(index)
   return Qt::ItemIsEnabled;
}

bool InstantMessagingModel::setData(const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED(index)
   Q_UNUSED(value)
   Q_UNUSED(role)
   if (index.column() == 0 && role == MESSAGE_IMAGE_ROLE   ) {
      m_lImages[index] = value;
   }
   return false;
}

void InstantMessagingModel::addIncommingMessage(QString from, QString message)
{
   InternalIM im;
   im.from = from;
   im.message = message;
   m_lMessages << im;
   emit dataChanged(index(m_lMessages.size() -1,0), index(m_lMessages.size()-1,0));
}

void InstantMessagingModel::addOutgoingMessage(QString message)
{
   InternalIM im;
   im.from = "Me";
   im.message = message;
   m_lMessages << im;
   emit dataChanged(index(m_lMessages.size() -1,0), index(m_lMessages.size()-1,0));
}