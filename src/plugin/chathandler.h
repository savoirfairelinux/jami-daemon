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

#include "observer.h"
#include "streamdata.h"
#include <string>
#include <map>

namespace jami {

using pluginMessagePtr = std::shared_ptr<JamiMessage>;
using chatSubjectPtr = std::shared_ptr<PublishObservable<pluginMessagePtr>>;

/**
 * @brief This abstract class is an API we need to implement from plugin side.
 * In other words, a plugin functionality that plays with messages, must start
 * from the implementation of this class.
 */
class ChatHandler
{
public:
    virtual ~ChatHandler() {}

    /**
     * @brief Should attach a chat subject (Observable) and the plugin data process (Observer).
     * @param subjectConnection accountId, peerId pair
     * @param subject chat Subject pointer
     */
    virtual void notifyChatSubject(std::pair<std::string, std::string>& subjectConnection,
                                   chatSubjectPtr subject)
        = 0;

    /**
     * @brief Returns a map with handler's name, iconPath, and pluginId.
     */
    virtual std::map<std::string, std::string> getChatHandlerDetails() = 0;

    /**
     * @brief Should detach a chat subject (Observable) and the plugin data process (Observer).
     * @param subject chat subject pointer
     */
    virtual void detach(chatSubjectPtr subject) = 0;

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

    /**
     * @brief Returns the dataPath of the plugin that created this ChatHandler.
     */
    std::string id() const { return id_; }

    /**
     * @brief Should be called by the ChatHandler creator to set the plugins id_ variable.
     */
    virtual void setId(const std::string& id) final { id_ = id; }

private:
    // Is the dataPath of the plugin that created this ChatHandler.
    std::string id_;
};
} // namespace jami
