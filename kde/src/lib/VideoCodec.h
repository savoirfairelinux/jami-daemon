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
#ifndef VIDEO_CODEC_H
#define VIDEO_CODEC_H

#include "typedefs.h"

//Qt
class QStringList;

//SFLPhone
class Call;
class Account;

///@class VideoCodec Codecs used for video calls
class LIB_EXPORT VideoCodec {
   public:
      static VideoCodec* getCodec(QString name);
      static VideoCodec* getCurrentCodec(Call* call);
      static QList<VideoCodec*> getCodecList();
      static QList<VideoCodec*> getActiveCodecList(Account* account);
      static void setActiveCodecList(Account* account, QStringList codecs);
      QString getName();
      QString getBitrate(); //Is the second field the ID?
   private:
      VideoCodec(QString codecName);
      ~VideoCodec(){};
      static void init();
      static QHash<QString,VideoCodec*> m_slCodecs;
      QString m_Name;
      QString m_Bitrate;
      static bool m_sInit;
};
#endif