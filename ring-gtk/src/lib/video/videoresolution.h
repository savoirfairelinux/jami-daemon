/****************************************************************************
 *   Copyright (C) 2014 by Savoir-Faire Linux                               *
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
#ifndef VIDEORESOLUTION_H
#define VIDEORESOLUTION_H

#include <QtCore/QAbstractListModel>
#include <QtCore/QSize>
#include "../typedefs.h"

class VideoRate;
class VideoChannel;
class VideoDevice;

///@struct VideoResolution Equivalent of "640x480"
class LIB_EXPORT VideoResolution : public QAbstractListModel {
   Q_OBJECT
   //Only VideoDevice can add validated rates
   friend class VideoDevice;
public:
   //Constructor
   VideoResolution(const QString& size, VideoChannel* chan);
   explicit VideoResolution();

   //Getter
   const QString name() const;
   const QList<VideoRate*> validRates() const;
   int relativeIndex() const;
   VideoRate* activeRate();
   bool setActiveRate(VideoRate* rate);
   bool setActiveRate(int index);
   int width() const;
   int height() const;
   QSize size() const;

   //Setters
   void setWidth(int width);
   void setHeight(int height);

   //Model
   QVariant      data     ( const QModelIndex& index, int role = Qt::DisplayRole     ) const;
   int           rowCount ( const QModelIndex& parent = QModelIndex()                ) const;
   Qt::ItemFlags flags    ( const QModelIndex& index                                 ) const;
   virtual bool  setData  ( const QModelIndex& index, const QVariant &value, int role)      ;


private:

   //Attributes
   QList<VideoRate*> m_lValidRates;
   VideoRate*        m_pCurrentRate;
   VideoChannel*     m_pChannel;
   QSize             m_Size;
};

#endif
