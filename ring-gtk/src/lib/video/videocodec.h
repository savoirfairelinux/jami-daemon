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
#ifndef VIDEO_CODEC_H
#define VIDEO_CODEC_H

#include "../typedefs.h"
#include <QtCore/QObject>

class Account;
class VideoCodec;

typedef QHash<QString,VideoCodec*> CodecHash;

///VideoCodec: Codecs used for video calls
class LIB_EXPORT VideoCodec : public QObject {
   Q_OBJECT
   friend class VideoCodecModel;
   public:
      //Properties
      Q_PROPERTY(QString name       READ name                          )
      Q_PROPERTY(uint    bitrate    READ bitrate    WRITE setBitrate   )
      Q_PROPERTY(bool    enabled    READ isEnabled  WRITE setEnabled   )
      Q_PROPERTY(QString parameters READ parameters WRITE setParamaters)

      //Consts
      class CodecFields {
      public:
         constexpr static const char* PARAMETERS = "parameters";
         constexpr static const char* ENABLED    = "enabled"   ;
         constexpr static const char* BITRATE    = "bitrate"   ;
         constexpr static const char* NAME       = "name"      ;
      };

      //Static setters
      static void setActiveCodecList(Account* account, QStringList codecs);

      //Getters
      QString name      () const;
      uint    bitrate   () const;
      bool    isEnabled () const;
      QString parameters() const;
      QMap<QString,QString> toMap() const;

      //Setters
      void setBitrate   (const uint     bitrate );
      void setEnabled   (const bool     enabled );
      void setParamaters(const QString& params  );

   private:
      //Constructor
      VideoCodec(const QString &codecName, uint bitRate, bool enabled);
      ~VideoCodec(){}

      //Attributes
      static CodecHash m_slCodecs;
      QString          m_Name;
      uint             m_Bitrate;
      bool             m_Enabled;
      static bool      m_sInit;
      QString          m_Parameters;
};

#endif
