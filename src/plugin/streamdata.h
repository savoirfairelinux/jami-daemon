/*
 *  Copyright (C) 2020-2021 Savoir-faire Linux Inc.
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

/**
 * @struct StreamData
 * @brief Contains information about an AV subject.
 * It's used by CallServicesManager.
 */
struct StreamData
{
    /**
     * @param callId
     * @param isReceived False if local audio/video streams
     * @param mediaType
     * @param peerId
     */
    StreamData(const std::string& callId,
               bool isReceived,
               const StreamType& mediaType,
               const std::string& peerId)
        : id {std::move(callId)}
        , direction {isReceived}
        , type {mediaType}
        , source {std::move(peerId)}
    {}
    // callId
    const std::string id;
    // False if local audio/video.
    const bool direction;
    // StreamType -> audio or video.
    const StreamType type;
    // peerId
    const std::string source;
};

/**
 * @struct JamiMessage
 * @brief Contains information about an exchanged message.
 * It's used by ChatServicesManager.
 */
struct JamiMessage
{
    /**
     * @param accId AccountId
     * @param pId peerId
     * @param isReceived False if local audio/video streams
     * @param dataMap Message contents
     * @param pPlugin True if message is created/modified by plugin code
     */
    JamiMessage(const std::string& accId,
                const std::string& pId,
                bool isReceived,
                std::map<std::string, std::string>& dataMap,
                bool pPlugin)
        : accountId {accId}
        , peerId {pId}
        , direction {isReceived}
        , data {dataMap}
        , fromPlugin {pPlugin}
    {}

    std::string accountId;
    std::string peerId;
    // True if it's a received message.
    const bool direction;
    std::map<std::string, std::string> data;
    // True if message is originated from Plugin code.
    bool fromPlugin;
    bool isSwarm {false};
    bool fromHistory {false};
};