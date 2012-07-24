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
#include "AudioCodecModel.h"

#include <QtCore/QDebug>

///Constructor
AudioCodecModel::AudioCodecModel(QObject* parent) : QAbstractListModel(parent) {

}

///Model data
QVariant AudioCodecModel::data(const QModelIndex& index, int role) const {
   if(index.column() == 0 && role == Qt::DisplayRole)
      return QVariant(m_lAudioCodecs[index.row()]->name);
   else if(index.column() == 0 && role == Qt::CheckStateRole)
         return QVariant(m_lEnabledCodecs[m_lAudioCodecs[index.row()]->id] ? Qt::Checked : Qt::Unchecked);
   else if (index.column() == 0 && role == AudioCodecModel::NAME_ROLE) {
      return m_lAudioCodecs[index.row()]->name;
   }
   else if (index.column() == 0 && role == AudioCodecModel::BITRATE_ROLE) {
      return m_lAudioCodecs[index.row()]->bitrate;
   }
   else if (index.column() == 0 && role == AudioCodecModel::SAMPLERATE_ROLE) {
      return m_lAudioCodecs[index.row()]->samplerate;
   }
   else if (index.column() == 0 && role == AudioCodecModel::ID_ROLE) {
      return m_lAudioCodecs[index.row()]->id;
   }
   return QVariant();
}

///Number of audio codecs
int AudioCodecModel::rowCount(const QModelIndex& parent) const {
   Q_UNUSED(parent)
   return m_lAudioCodecs.size();
}

///Model flags
Qt::ItemFlags AudioCodecModel::flags(const QModelIndex& index) const {
   if (index.column() == 0)
      return QAbstractItemModel::flags(index) | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
   return QAbstractItemModel::flags(index);
}

///Set audio codec data
bool AudioCodecModel::setData( const QModelIndex& index, const QVariant &value, int role) {
   if (index.column() == 0 && role == AudioCodecModel::NAME_ROLE) {
      m_lAudioCodecs[index.row()]->name = value.toString();
      emit dataChanged(index, index);
      return true;
   }
   else if (index.column() == 0 && role == AudioCodecModel::BITRATE_ROLE) {
      m_lAudioCodecs[index.row()]->bitrate = value.toString();
      emit dataChanged(index, index);
      return true;
   }
   else if(index.column() == 0 && role == Qt::CheckStateRole) {
      m_lEnabledCodecs[m_lAudioCodecs[index.row()]->id] = value.toBool();
      emit dataChanged(index, index);
      return true;
   }
   else if (index.column() == 0 && role == AudioCodecModel::SAMPLERATE_ROLE) {
      m_lAudioCodecs[index.row()]->samplerate = value.toString();
      emit dataChanged(index, index);
      return true;
   }
   else if (index.column() == 0 && role == AudioCodecModel::ID_ROLE) {
      m_lAudioCodecs[index.row()]->id = value.toInt();
      emit dataChanged(index, index);
      return true;
   }
   return false;
}

///Add a new audio codec
QModelIndex AudioCodecModel::addAudioCodec() {
   m_lAudioCodecs << new AudioCodecData;
   emit dataChanged(index(m_lAudioCodecs.size()-1,0), index(m_lAudioCodecs.size()-1,0));
   return index(m_lAudioCodecs.size()-1,0);
}

///Remove audio codec at 'idx'
void AudioCodecModel::removeAudioCodec(QModelIndex idx) {
   qDebug() << "REMOVING" << idx.row() << m_lAudioCodecs.size();
   if (idx.isValid()) {
      m_lAudioCodecs.removeAt(idx.row());
      emit dataChanged(idx, index(m_lAudioCodecs.size()-1,0));
      qDebug() << "DONE" << m_lAudioCodecs.size();
   }
   else {
      qDebug() << "Failed to remove an invalid audio codec";
   }
}

///Remove everything
void AudioCodecModel::clear()
{
   foreach(AudioCodecData* data, m_lAudioCodecs) {
      delete data;
   }
   m_lAudioCodecs.clear();
   m_lEnabledCodecs.clear();
}

///Increase codec priority
bool AudioCodecModel::moveUp(QModelIndex idx)
{
   if(idx.row() > 0 && idx.row() <= rowCount()) {
      AudioCodecData* data = m_lAudioCodecs[idx.row()];
      m_lAudioCodecs.removeAt(idx.row());
      m_lAudioCodecs.insert(idx.row() - 1, data);
      emit dataChanged(index(idx.row() - 1, 0, QModelIndex()), index(idx.row(), 0, QModelIndex()));
      return true;
   }
   return false;
}

///Decrease codec priority
bool AudioCodecModel::moveDown(QModelIndex idx)
{
   if(idx.row() >= 0 && idx.row() < rowCount()) {
      AudioCodecData* data = m_lAudioCodecs[idx.row()];
      m_lAudioCodecs.removeAt(idx.row());
      m_lAudioCodecs.insert(idx.row() + 1, data);
      emit dataChanged(index(idx.row(), 0, QModelIndex()), index(idx.row() + 1, 0, QModelIndex()));
      return true;
   }
   return false;
}
