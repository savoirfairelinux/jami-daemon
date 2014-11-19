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
#include "instantmessagingmodel.h"

#include "callmodel.h"
#include "dbus/callmanager.h"
#include "call.h"
#include "contact.h"
#include "phonenumber.h"

InstantMessagingModelManager* InstantMessagingModelManager::m_spInstance  = nullptr;

///Signleton
InstantMessagingModelManager* InstantMessagingModelManager::instance()
{
   if (!m_spInstance) {
      m_spInstance = new InstantMessagingModelManager();
   }
   return m_spInstance;
}

void InstantMessagingModelManager::init() {
   instance();
}

///Constructor
InstantMessagingModelManager::InstantMessagingModelManager() : QObject(nullptr)
{
   CallManagerInterface& callManager = DBus::CallManager::instance();
   connect(&callManager, SIGNAL(incomingMessage(QString,QString,QString)), this, SLOT(newMessage(QString,QString,QString)));
}

///Called when a new message is incoming
void InstantMessagingModelManager::newMessage(QString callId, QString from, QString message)
{
   if (!m_lModels[callId] && CallModel::instance()) {
      Call* call = CallModel::instance()->getCall(callId);
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

///Singleton
InstantMessagingModel* InstantMessagingModelManager::getModel(Call* call) {
   const QString key = call->id();
   if (!m_lModels[key]) {
      m_lModels[key] = new InstantMessagingModel(call);
      emit newMessagingModel(call,m_lModels[key]);
   }
   return m_lModels[key];
}

///Constructor
InstantMessagingModel::InstantMessagingModel(Call* call, QObject* par) : QAbstractListModel(par),m_pCall(call)
{
   //QStringList callList = callManager.getCallList();
   QHash<int, QByteArray> roles = roleNames();
   roles.insert(InstantMessagingModel::Role::TYPE    ,QByteArray("type"));
   roles.insert(InstantMessagingModel::Role::FROM    ,QByteArray("from"));
   roles.insert(InstantMessagingModel::Role::TEXT    ,QByteArray("text"));
   roles.insert(InstantMessagingModel::Role::IMAGE   ,QByteArray("image"));
   roles.insert(InstantMessagingModel::Role::CONTACT ,QByteArray("contact"));
   setRoleNames(roles);
}

///Get data from the model
QVariant InstantMessagingModel::data( const QModelIndex& idx, int role) const
{
   if (idx.column() == 0) {
      switch (role) {
         case Qt::DisplayRole:
            return QVariant(m_lMessages[idx.row()].message);
            break;
         case InstantMessagingModel::Role::TYPE:
            return QVariant(m_lMessages[idx.row()].message);
            break;
         case InstantMessagingModel::Role::FROM:
            return QVariant(m_lMessages[idx.row()].from);
            break;
         case InstantMessagingModel::Role::TEXT:
            return INCOMMING_IM;
            break;
         case InstantMessagingModel::Role::CONTACT:
            if (m_pCall->peerPhoneNumber()->contact()) {
               return QVariant();
            }
            break;
         case InstantMessagingModel::Role::IMAGE: {
            if (m_lImages.find(idx) != m_lImages.end())
               return m_lImages[idx];
            const Contact* c = m_pCall->peerPhoneNumber()->contact();
            if (c && c->photo()) {
               return QVariant::fromValue<void*>((void*)c->photo());
            }
            return QVariant();
            break;
         }
         default:
            break;
      }
   }
   return QVariant();
}

///Number of row
int InstantMessagingModel::rowCount(const QModelIndex& parentIdx) const
{
   Q_UNUSED(parentIdx)
   return m_lMessages.size();
}

///Model flags
Qt::ItemFlags InstantMessagingModel::flags(const QModelIndex& idx) const
{
   Q_UNUSED(idx)
   return Qt::ItemIsEnabled;
}

///Set model data
bool InstantMessagingModel::setData(const QModelIndex& idx, const QVariant &value, int role)
{
   Q_UNUSED(idx)
   Q_UNUSED(value)
   Q_UNUSED(role)
   if (idx.column() == 0 && role == InstantMessagingModel::Role::IMAGE   ) {
      m_lImages[idx] = value;
   }
   return false;
}

///Add new incoming message (to be used internally)
void InstantMessagingModel::addIncommingMessage(QString from, QString message)
{
   InternalIM im;
   im.from    = from;
   im.message = message;
   m_lMessages << im;
   emit dataChanged(index(m_lMessages.size() -1,0), index(m_lMessages.size()-1,0));
}

///Add new outgoing message (to be used internally and externally)
void InstantMessagingModel::addOutgoingMessage(QString message)
{
   InternalIM im;
   im.from    = tr("Me");
   im.message = message;
   m_lMessages << im;
   emit dataChanged(index(m_lMessages.size() -1,0), index(m_lMessages.size()-1,0));
}
