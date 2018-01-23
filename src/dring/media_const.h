/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
 *
 *  Author: Eloi Bail <eloi.bail@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */
#ifndef DRING_MEDIA_H
#define DRING_MEDIA_H

namespace DRing {

namespace Media {

// Supported MRL schemes
namespace VideoProtocolPrefix {

constexpr static const char* NONE       = "";
constexpr static const char* DISPLAY    = "display";
constexpr static const char* FILE       = "file";
constexpr static const char* CAMERA     = "camera";
constexpr static const char* SEPARATOR  = "://";
}

namespace Details {

constexpr static char MEDIA_TYPE_AUDIO[] = "MEDIA_TYPE_AUDIO";
constexpr static char MEDIA_TYPE_VIDEO[] = "MEDIA_TYPE_VIDEO";
}

} //namespace DRing::Media

} //namespace DRing

#endif
