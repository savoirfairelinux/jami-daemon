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
#include "observer.h"
#include <map>
#include <string>

namespace jami {
struct ConversationMessage
{
    ConversationMessage(const std::string& author,
                        const std::string& to,
                        std::map<std::string, std::string>& dataMap)
        : author_ {author}
        , to_ {to}
        , data_ {dataMap}
    {}
    std::string author_;
    std::string to_;
    std::map<std::string, std::string> data_;
};

using ConvMsgPtr = std::shared_ptr<ConversationMessage>;

using strMapSubjectPtr = std::shared_ptr<PublishObservable<ConvMsgPtr>>;

class ConversationHandler
{
public:
    virtual ~ConversationHandler()                                                   = default;
    virtual void notifyStrMapSubject(const bool direction, strMapSubjectPtr subject) = 0;
};
} // namespace jami
