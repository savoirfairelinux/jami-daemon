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

#include "observer.h"
#include "streamdata.h"
#include <string>
#include <map>

namespace jami {

using pluginMessagePtr = std::shared_ptr<JamiMessage>;
using chatSubjectPtr = std::shared_ptr<PublishObservable<pluginMessagePtr>>;

class ChatHandler
{
public:
    virtual ~ChatHandler() = default;
    virtual void notifyChatSubject(std::pair<std::string, std::string>& subjectConnection,
                                   chatSubjectPtr subject)
        = 0;
    virtual std::map<std::string, std::string> getChatHandlerDetails() = 0;
    virtual void detach(chatSubjectPtr subject) = 0;
    virtual void setPreferenceAttribute(const std::string& key, const std::string& value) = 0;
    virtual bool preferenceMapHasKey(const std::string& key) = 0;

    /**
     * @brief id
     * The id is the path of the plugin that created this MediaHandler
     * @return
     */
    std::string id() const { return id_; }
    virtual void setId(const std::string& id) final { id_ = id; }

private:
    std::string id_;
};
} // namespace jami
