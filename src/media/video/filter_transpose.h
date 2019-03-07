/*
 *  Copyright (C) 2019 Savoir-faire Linux Inc.
 *
 *  File: filter_transpose.h
 *
 *  Author: Denys VIDAL <denys.vidal@savoirfairelinux.com>
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

#pragma once

#include "../media_filter.h"

namespace ring {
namespace video {

std::unique_ptr<MediaFilter>
getTransposeFilter(int rotation, std::string inputName, int width, int height, int format, bool rescale);

}
}
