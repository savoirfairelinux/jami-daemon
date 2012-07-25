/****************************************************************************
 *   Copyright (C) 2012 by Savoir-Faire Linux                               *
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
#ifndef VIDEO_CODEC_H
#define VIDEO_CODEC_H

#include "typedefs.h"
#include <QtCore/QAbstractListModel>

//Qt
class QStringList;

//SFLPhone
class Call;
class Account;

//Typedef
class VideoCodec;
typedef QHash<QString,VideoCodec*> CodecHash;

///Abstract model for managing account video codec list
class LIB_EXPORT VideoCodecModel : public QAbstractListModel {
   Q_OBJECT
   friend class Account;
public:
   //Roles
   static const int BITRATE_ROLE = 101;

   //Model functions
   QVariant      data     ( const QModelIndex& index, int role = Qt::DisplayRole     ) const;
   int           rowCount ( const QModelIndex& parent = QModelIndex()                ) const;
   Qt::ItemFlags flags    ( const QModelIndex& index                                 ) const;
   virtual bool  setData  ( const QModelIndex& index, const QVariant &value, int role)      ;
   
   void reload();
   void save();
   bool moveUp  (QModelIndex idx);
   bool moveDown(QModelIndex idx);
   
private:
   //Private constructor, can only be called by 'Account'
   VideoCodecModel(Account* account);

   //Attrbutes
   QList<VideoCodec*> m_lCodecs;
   Account*           m_pAccount;
};

///VideoCodec: Codecs used for video calls
class LIB_EXPORT VideoCodec {
   friend class VideoCodecModel;
   public:
      //Static setters
      static void setActiveCodecList(Account* account, QStringList codecs);

      //Getters
      QString getName   () const;
      uint    getBitrate() const;
      bool    getEnabled() const;

      //Setters
      void setBitrate(const uint bitrate);
      void setEnabled(const bool enabled);
      
   private:
      //Constructor
      VideoCodec(QString codecName, uint bitRate, bool enabled);
      ~VideoCodec(){};

      //Attributes
      static CodecHash m_slCodecs;
      QString m_Name;
      uint m_Bitrate;
      bool    m_Enabled;
      static bool m_sInit;
};
#endif
