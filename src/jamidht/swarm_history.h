/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "jami/conversation_interface.h"

#include <condition_variable>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

namespace jami {

/**
 * The history of a conversation or of a circle channel as clients see it: the displayed messages
 * in a linear order, newest first, with their reactions and editions folded into them.
 */
struct SwarmHistory
{
    using Message = std::shared_ptr<libjami::SwarmMessage>;
    // Visits (commit id, is a merge) from the head, in loading order; true stops the walk.
    using Visitor = std::function<bool(const std::string&, bool)>;

    // While loading the history, we need to avoid:
    // - reloading history (can just be ignored)
    // - adding new commits (should wait for history to be loaded)
    std::mutex mutex {};
    std::condition_variable cv {};
    bool loading {false};
    std::list<Message> messageList {};
    std::map<std::string, Message> quickAccess {};
    std::map<std::string, std::list<Message>> pendingEditions {};
    std::map<std::string, std::list<std::map<std::string, std::string>>> pendingReactions {};

    void clear();

    /**
     * Adds a message, newer than the known ones when received and older otherwise, and signals
     * the changes of what is displayed to the clients of the account.
     * @return whether an older message is now displayed
     */
    bool add(const std::string& accountId, const std::string& id, const Message& message, bool received);

    // Signals the displayed message a received edition changed.
    void signalEdited(const std::string& accountId, const std::string& id, const std::string& editedId);

    /**
     * After a non fast-forward merge, reorder the newest messages so that they match the log
     * order, and notify clients of moved messages.
     * @param newIds commits brought by the merge
     * @param walk   walks the log from its head
     */
    void relinearize(const std::string& accountId,
                     const std::string& id,
                     std::set<std::string> newIds,
                     const std::function<void(const Visitor&)>& walk);

private:
    void addReaction(const std::string& accountId, const std::string& id, const Message& message);
    void addEdition(const std::string& accountId, const std::string& id, const Message& message, bool received);
    bool addMessage(const std::string& accountId, const std::string& id, const Message& message, bool received);
};

} // namespace jami
