/*
 *  Copyright (C) 2020-2023 Savoir-faire Linux Inc.
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

#include "streamdata.h"
#include "observer.h"

#include <string>
#include <memory>
#include <map>

extern "C" {
struct AVFrame;
}

namespace jami {

using avSubjectPtr = std::shared_ptr<Observable<AVFrame*>>;

/**
 * @class MediaHandler
 * @brief It's the base object of the CallMediaHandler
 */
class MediaHandler
{
public:
    virtual ~MediaHandler() = default;

    /**
     * @brief Returns the dataPath of the plugin that created this MediaHandler.
     */
    std::string id() const { return id_; }

    /**
     * @brief Should be called by the MediaHandler creator to set the plugins id_ variable
     * with dataPath.
     */
    virtual void setId(const std::string& id) final { id_ = id; }

private:
    // Must be set with plugin's dataPath.
    std::string id_;
};

/**
 * @class  CallMediaHandler
 * @brief This abstract class is an API we need to implement from plugin side.
 * In other words, a plugin functionality that plays with audio or video, must start
 * from the implementation of this class.
 */
class CallMediaHandler : public MediaHandler
{
public:
    /**
     * @brief Should attach a AVSubject (Observable) to the plugin data process (Observer).
     * @param data
     * @param subject
     */
    virtual void notifyAVFrameSubject(const StreamData& data, avSubjectPtr subject) = 0;

    /**
     * @brief Should return a map with handler's name, iconPath, pluginId, attached, and dataType.
     * Daemon expects:
     *      "attached" -> 1 if handler is attached or a list of attached calls;
     *      "dataType" -> 1 if data processed is video;
     *      "dataType" -> 0 if data processed is audio;
     * @return Map with CallMediaHandler details.
     */
    virtual std::map<std::string, std::string> getCallMediaHandlerDetails() = 0;

    /**
     * @brief Should detach the plugin data process (Observer).
     */
    virtual void detach(avSubjectPtr subject = nullptr) = 0;

    /**
     * @brief If a preference can be changed without the need to reload the plugin, it
     * should be done through this function.
     * @param key
     * @param value
     */
    virtual void setPreferenceAttribute(const std::string& key, const std::string& value) = 0;

    /**
     * @brief If a preference can be changed without the need to reload the plugin, this function
     * should return True.
     * @param key
     * @return True if preference can be changed through setPreferenceAttribute method.
     */
    virtual bool preferenceMapHasKey(const std::string& key) = 0;
};
} // namespace jami
