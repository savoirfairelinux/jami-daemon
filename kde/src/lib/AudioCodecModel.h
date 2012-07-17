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
#ifndef AUDIO_CODEC_MODEL_H
#define AUDIO_CODEC_MODEL_H

#include <QtCore/QString>
#include <QtCore/QAbstractListModel>
#include "typedefs.h"

///AudioCodecModel: A model for account audio codec
class LIB_EXPORT AudioCodecModel : public QAbstractListModel {
   Q_OBJECT
public:
   //friend class Account;
   //Roles
   static const int ID_ROLE         = 103;
   static const int NAME_ROLE       = 100;
   static const int BITRATE_ROLE    = 101;
   static const int SAMPLERATE_ROLE = 102;

   //Constructor
   AudioCodecModel(QObject* parent =nullptr);

   //Abstract model member
   QVariant data(const QModelIndex& index, int role = Qt::DisplayRole ) const;
   int rowCount(const QModelIndex& parent = QModelIndex()             ) const;
   Qt::ItemFlags flags(const QModelIndex& index                       ) const;
   virtual bool setData(const QModelIndex& index, const QVariant &value, int role);

   //Mutator
   QModelIndex addAudioCodec();
   void removeAudioCodec(QModelIndex idx);
   void clear();
   bool moveUp(QModelIndex idx);
   bool moveDown(QModelIndex idx);

private:
   ///@struct AudioCodecData store audio codec informations
   struct AudioCodecData {
      int              id        ;
      QString          name      ;
      QString          bitrate   ;
      QString          samplerate;
   };
   
   //Attributes
   QList<AudioCodecData*> m_lAudioCodecs;
   QMap<int,bool>  m_lEnabledCodecs;

};

#endif