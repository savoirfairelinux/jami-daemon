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
#ifndef AUDIO_CODEC_MODEL_H
#define AUDIO_CODEC_MODEL_H

#include <QtCore/QString>
#include <QtCore/QAbstractListModel>
#include "typedefs.h"

class Account;

///AudioCodecModel: A model for account audio codec
class LIB_EXPORT AudioCodecModel : public QAbstractListModel {
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
public:
   //friend class Account;
   //Roles
   enum Role {
      ID         = 103,
      NAME       = 100,
      BITRATE    = 101,
      SAMPLERATE = 102,
   };

   //Constructor
   explicit AudioCodecModel(Account* account);
   virtual ~AudioCodecModel();

   //Abstract model member
   QVariant data        (const QModelIndex& index, int role = Qt::DisplayRole      ) const;
   int rowCount         (const QModelIndex& parent = QModelIndex()                 ) const;
   Qt::ItemFlags flags  (const QModelIndex& index                                  ) const;
   virtual bool setData (const QModelIndex& index, const QVariant &value, int role );

   //Mutator
   QModelIndex addAudioCodec();
   Q_INVOKABLE void removeAudioCodec ( QModelIndex idx );
   Q_INVOKABLE bool moveUp           ( QModelIndex idx );
   Q_INVOKABLE bool moveDown         ( QModelIndex idx );
   Q_INVOKABLE void clear            (                 );
   Q_INVOKABLE void reload           (                 );
   Q_INVOKABLE void save             (                 );

private:
   ///@struct AudioCodecData store audio codec information
   struct AudioCodecData {
      int              id        ;
      QString          name      ;
      QString          bitrate   ;
      QString          samplerate;
   };

   //Attributes
   QList<AudioCodecData*> m_lAudioCodecs  ;
   QMap<int,bool>         m_lEnabledCodecs;
   Account*               m_pAccount      ;

   //Helpers
   bool findCodec(int id);
};
Q_DECLARE_METATYPE(AudioCodecModel*)

#endif
