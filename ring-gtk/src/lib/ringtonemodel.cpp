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
#include "ringtonemodel.h"

//Qt
#include <QtCore/QTimer>

//SFLphone
#include "dbus/configurationmanager.h"
#include "dbus/callmanager.h"
#include "account.h"

RingToneModel::RingToneModel(Account* a) : QAbstractTableModel(a),m_pAccount(a),m_pTimer(nullptr),
m_pCurrent(nullptr)
{
   ConfigurationManagerInterface& configurationManager = DBus::ConfigurationManager::instance();
   QMap<QString,QString> m_hRingtonePath = configurationManager.getRingtoneList();
   QMutableMapIterator<QString, QString> iter(m_hRingtonePath);
   while (iter.hasNext()) {
      iter.next();
      QFileInfo fileinfo(iter.key());
      RingToneInfo* info = new RingToneInfo();
      info->name = iter.value();
      info->path = fileinfo.absoluteFilePath();
      m_lRingTone << info;
   }
}

RingToneModel::~RingToneModel()
{
   while (m_lRingTone.size()) {
      RingToneInfo* ringtone = m_lRingTone[0];
      m_lRingTone.removeAt(0);
      delete ringtone;
   }
}

QVariant RingToneModel::data( const QModelIndex& index, int role ) const
{
   if (!index.isValid())
      return QVariant();
   RingToneInfo* info = m_lRingTone[index.row()];
   switch (index.column()) {
      case 0:
         switch (role) {
            case Qt::DisplayRole:
               return info->name;
            case Role::IsPlaying:
               return info->isPlaying;
            case Role::FullPath:
               return info->path;
         };
         break;
      case 1:
         switch (role) {
            case Role::FullPath:
               return info->path;
         };
         break;
   };
   return QVariant();
}

int RingToneModel::rowCount( const QModelIndex& parent ) const
{
   if (!parent.isValid())
      return m_lRingTone.size();
   return 0;
}

int RingToneModel::columnCount( const QModelIndex& parent ) const
{
   if (parent.isValid())
      return 0;
   return 2; //Name, then an empty one for widgets
}

Qt::ItemFlags RingToneModel::flags( const QModelIndex& index ) const
{
   if (index.isValid() && !index.parent().isValid())
      return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
   return Qt::NoItemFlags;
}

///This is a read only model
bool RingToneModel::setData( const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED(index)
   Q_UNUSED(value)
   Q_UNUSED(role )
   return false;
}

QString RingToneModel::currentRingTone() const
{
   return QFileInfo(m_pAccount->ringtonePath()).absoluteFilePath();
}

QModelIndex RingToneModel::currentIndex() const
{
   const QString rt = currentRingTone();
   for (int i=0;i<m_lRingTone.size();i++) {
      RingToneInfo* info = m_lRingTone[i];
      if (info->path == rt)
         return index(i,0);
   }
   return QModelIndex();
}

void RingToneModel::play(const QModelIndex& idx)
{
   if (idx.isValid()) {
      RingToneInfo* info = m_lRingTone[idx.row()];
      if (m_pCurrent && info == m_pCurrent) {
         slotStopTimer();
         return;
      }
      CallManagerInterface& callManager = DBus::CallManager::instance();
      Q_NOREPLY callManager.startRecordedFilePlayback(info->path);
      if (!m_pTimer) {
         m_pTimer = new QTimer(this);
         m_pTimer->setInterval(10000);
         connect(m_pTimer,SIGNAL(timeout()),this,SLOT(slotStopTimer()));
      }
      else if (m_pTimer->isActive()) {
         m_pTimer->stop();
      }
      m_pTimer->start();
      info->isPlaying = true;
      emit dataChanged(index(idx.row(),0),index(idx.row(),1));
      m_pCurrent = info;
   }
}

void RingToneModel::slotStopTimer()
{
   if (m_pCurrent) {
      CallManagerInterface& callManager = DBus::CallManager::instance();
      callManager.stopRecordedFilePlayback(m_pCurrent->path);
      m_pCurrent->isPlaying = false;
      const QModelIndex& idx = index(m_lRingTone.indexOf(m_pCurrent),0);
      emit dataChanged(idx,index(idx.row(),1));
      m_pCurrent = nullptr;
      m_pTimer->stop();
   }
}
