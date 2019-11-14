/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
#include <string>

enum class StreamType { audio, video };

struct StreamData {
    StreamData(const std::string& i, bool d, StreamType&& t, const std::string& s) :
        id{std::move(i)}, direction{d}, type{t}, source{std::move(s)} {}
    const std::string id;
    const bool direction;
    const StreamType type;
    const std::string source;
};
