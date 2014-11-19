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
#ifndef VIDEORATE_H
#define VIDEORATE_H

#include "../typedefs.h"

class VideoResolution;

///@typedef VideoRate The rate for a device
class LIB_EXPORT VideoRate
{
   //Can only be created by VideoDevice
   friend class VideoDevice;

public:
   virtual ~VideoRate() {}
   QString name() const;
   int relativeIndex();

private:
   VideoRate(const VideoResolution* res,const QString& name) :
      m_Name(name),m_pResolution(res) {}
   QString m_Name;
   const VideoResolution* m_pResolution;
};

#endif
