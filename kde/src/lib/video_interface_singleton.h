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

#ifndef VIDEO_INTERFACE_SINGLETON_H
#define VIDEO_INTERFACE_SINGLETON_H

#include "src/lib/video_dbus_interface.h"
#include "typedefs.h"

/**
 * @author Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>
 */
class LIB_EXPORT VideoInterfaceSingleton
{

private:
   static VideoInterface* interface;

public:
   static VideoInterface& getInstance();

};

#endif
