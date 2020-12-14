/*
 *  Copyright (C) 2020 Savoir-faire Linux Inc.
 *
 *  Author: Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
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
#include <map>

enum class StreamType { audio, video };

struct StreamData // for calls
{
    StreamData(const std::string& i, bool d, StreamType&& t, const std::string& s)
        : id {std::move(i)}
        , direction {d}
        , type {t}
        , source {std::move(s)}
    {}
    const std::string id;
    const bool direction; // 0 when local; 1 when received
    const StreamType type;
    const std::string source;
};

struct JamiMessage // for chat
{
    JamiMessage(const std::string& accId,
                const std::string& pId,
                bool direction,
                std::map<std::string, std::string>& dataMap,
                bool pPlugin)
        : accountId {accId}
        , peerId {pId}
        , direction {direction}
        , data {dataMap}
        , fromPlugin {pPlugin}
    {}

    std::string accountId; // accountID
    std::string peerId;    // peer
    const bool direction;  // 0 -> send; 1 -> received
    std::map<std::string, std::string> data;
    bool fromPlugin;
    bool isSwarm {false};
    bool fromHistory {false};
};