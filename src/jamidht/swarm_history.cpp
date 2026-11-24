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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "swarm_history.h"

#include "client/jami_signal.h"
#include "commit_message.h"

#include <algorithm>
#include <vector>

namespace jami {

void
SwarmHistory::clear()
{
    messageList.clear();
    quickAccess.clear();
    pendingEditions.clear();
    pendingReactions.clear();
}

bool
SwarmHistory::add(const std::string& accountId, const std::string& id, const Message& message, bool received)
{
    quickAccess[message->id] = message;
    auto reactToIt = message->body.find(CommitKey::REACT_TO);
    auto editIt = message->body.find(CommitKey::EDIT);
    if (reactToIt != message->body.end() && !reactToIt->second.empty()) {
        addReaction(accountId, id, message);
    } else if (editIt != message->body.end() && !editIt->second.empty()) {
        addEdition(accountId, id, message, received);
    } else {
        return addMessage(accountId, id, message, received);
    }
    return false;
}

void
SwarmHistory::signalEdited(const std::string& accountId, const std::string& id, const std::string& editedId)
{
    libjami::SwarmMessage edited;
    {
        std::lock_guard lk(mutex);
        if (auto original = quickAccess.find(editedId); original != quickAccess.end())
            edited = *original->second;
    }
    if (!edited.id.empty() && !edited.body.contains(CommitKey::REACT_TO))
        emitSignal<libjami::ConversationSignal::SwarmMessageUpdated>(accountId, id, edited);
}

void
SwarmHistory::addReaction(const std::string& accountId, const std::string& id, const Message& sharedCommit)
{
    auto it = quickAccess.find(sharedCommit->body.at(CommitKey::REACT_TO));
    auto peditIt = pendingEditions.find(sharedCommit->id);
    if (peditIt != pendingEditions.end()) {
        sharedCommit->body[CommitKey::BODY] = peditIt->second.front()->body[CommitKey::BODY];
        if (sharedCommit->body.at(CommitKey::BODY).empty())
            return;
        pendingEditions.erase(peditIt);
    }
    if (it != quickAccess.end()) {
        // A deleted message (its body, or tid for a file, has been emptied by an edition) displays
        // no reactions.
        const auto& target = it->second;
        const std::string& bodyKey = (target->type == CommitType::DATA_TRANSFER) ? CommitKey::TID : CommitKey::BODY;
        auto bodyIt = target->body.find(bodyKey);
        if (bodyIt != target->body.end() && bodyIt->second.empty())
            return;
        target->reactions.emplace_back(sharedCommit->body);
        emitSignal<libjami::ConversationSignal::ReactionAdded>(accountId, id, target->id, sharedCommit->body);
    } else {
        pendingReactions[sharedCommit->body.at(CommitKey::REACT_TO)].emplace_back(sharedCommit->body);
    }
}

void
SwarmHistory::addEdition(const std::string& accountId, const std::string& id, const Message& sharedCommit, bool received)
{
    auto editId = sharedCommit->body.at(CommitKey::EDIT);
    auto it = quickAccess.find(editId);
    if (it != quickAccess.end()) {
        auto baseCommit = it->second;
        if (baseCommit) {
            auto itReact = baseCommit->body.find(CommitKey::REACT_TO);
            std::string toReplace = (baseCommit->type == CommitType::DATA_TRANSFER) ? CommitKey::TID : CommitKey::BODY;
            // An edition need not carry the field it replaces: retiring a
            // collaborative document says nothing but which commit it retires.
            // Absent means empty, which is what an edition to nothing already
            // means everywhere below.
            std::string body;
            if (auto itBody = sharedCommit->body.find(toReplace); itBody != sharedCommit->body.end())
                body = itBody->second;
            // Edit reaction
            if (itReact != baseCommit->body.end()) {
                baseCommit->body[toReplace] = body; // Replace body if pending
                it = quickAccess.find(itReact->second);
                auto itPending = pendingReactions.find(itReact->second);
                if (it != quickAccess.end()) {
                    baseCommit = it->second; // Base commit
                    auto itPreviousReact = std::find_if(baseCommit->reactions.begin(),
                                                        baseCommit->reactions.end(),
                                                        [&](const auto& reaction) {
                                                            return reaction.at("id") == editId;
                                                        });
                    if (itPreviousReact != baseCommit->reactions.end()) {
                        (*itPreviousReact)[toReplace] = body;
                        if (body.empty()) {
                            baseCommit->reactions.erase(itPreviousReact);
                            emitSignal<libjami::ConversationSignal::ReactionRemoved>(accountId,
                                                                                     id,
                                                                                     baseCommit->id,
                                                                                     editId);
                        }
                    }
                } else if (itPending != pendingReactions.end()) {
                    // Else edit if pending
                    auto itReaction = std::find_if(itPending->second.begin(),
                                                   itPending->second.end(),
                                                   [&](const auto& reaction) { return reaction.at("id") == editId; });
                    if (itReaction != itPending->second.end()) {
                        (*itReaction)[toReplace] = body;
                        if (body.empty())
                            itPending->second.erase(itReaction);
                    }
                } else {
                    // Add to pending edtions
                    received ? pendingEditions[editId].emplace_front(sharedCommit)
                             : pendingEditions[editId].emplace_back(sharedCommit);
                }
            } else {
                // Normal message
                auto editionBody = it->second->body;
                // Tag the superseded body with the commit id that introduced it, so the
                // client can look up per-edition plugin overwrites via quickAccess.
                editionBody["id"] = it->second->latestEditionId.empty() ? it->second->id : it->second->latestEditionId;
                it->second->editions.emplace(it->second->editions.begin(), std::move(editionBody));
#ifdef ENABLE_PLUGIN
                if (const auto boIt = it->second->pluginData.find("bodyOverwrite");
                    boIt != it->second->pluginData.end()) {
                    it->second->editions.front()["bodyOverwrite"] = boIt->second;
                }
#endif
                it->second->body[toReplace] = sharedCommit->body[toReplace];
                it->second->latestEditionId = sharedCommit->id;
                if (toReplace == CommitKey::TID) {
                    // Avoid to replace fileId in client
                    it->second->body["fileId"] = "";
                }
                // Remove reactions
                if (sharedCommit->body.at(toReplace).empty())
                    it->second->reactions.clear();
            }
        }
    } else {
        received ? pendingEditions[editId].emplace_front(sharedCommit)
                 : pendingEditions[editId].emplace_back(sharedCommit);
    }
}

bool
SwarmHistory::addMessage(const std::string& accountId, const std::string& id, const Message& sharedCommit, bool received)
{
    if (received) {
        // For a received message, we place it at the beginning of the list
        if (!messageList.empty())
            sharedCommit->linearizedParent = (*messageList.begin())->id;
        messageList.emplace_front(sharedCommit);
    } else {
        // For a loaded message, we load from newest to oldest
        // So we change the parent of the last message.
        if (!messageList.empty())
            (*messageList.rbegin())->linearizedParent = sharedCommit->id;
        messageList.emplace_back(sharedCommit);
    }
    // Handle pending reactions/editions
    auto reactIt = pendingReactions.find(sharedCommit->id);
    if (reactIt != pendingReactions.end()) {
        for (const auto& commitBody : reactIt->second)
            sharedCommit->reactions.emplace_back(commitBody);
        pendingReactions.erase(reactIt);
    }
    auto peditIt = pendingEditions.find(sharedCommit->id);
    if (peditIt != pendingEditions.end()) {
        std::string toReplace = (sharedCommit->type == CommitType::DATA_TRANSFER) ? CommitKey::TID : CommitKey::BODY;
        // Pending editions are stored newest first: replay them oldest first, as addEdition would.
        for (auto it = peditIt->second.rbegin(); it != peditIt->second.rend(); ++it) {
            const auto& editCommit = *it;
            // Retiring a collaborative document carries no body: absent means empty.
            std::string body;
            if (auto itBody = editCommit->body.find(toReplace); itBody != editCommit->body.end())
                body = itBody->second;
            auto editionBody = sharedCommit->body;
            editionBody["id"] = sharedCommit->latestEditionId.empty() ? sharedCommit->id
                                                                      : sharedCommit->latestEditionId;
            sharedCommit->editions.emplace(sharedCommit->editions.begin(), std::move(editionBody));
            sharedCommit->body[toReplace] = body;
            sharedCommit->latestEditionId = editCommit->id;
            if (toReplace == CommitKey::TID)
                sharedCommit->body["fileId"] = "";
            if (body.empty())
                sharedCommit->reactions.clear();
        }
        pendingEditions.erase(peditIt);
    }
    // Announce to client
    if (received)
        emitSignal<libjami::ConversationSignal::SwarmMessageReceived>(accountId, id, *sharedCommit);
    return !received;
}

void
SwarmHistory::relinearize(const std::string& accountId,
                          const std::string& id,
                          std::set<std::string> newIds,
                          const std::function<void(const Visitor&)>& walk)
{
    if (newIds.empty())
        return;

    // Walk the log from HEAD, in loading order, until every announced commit has
    // been seen plus one more commit, the anchor. Everything older than the anchor
    // is left alone, so the anchor must also be older than every message currently
    // shown: commits sharing a timestamp are not guaranteed to keep their relative
    // order once the graph changes.
    auto& list = messageList;
    auto oldIt = list.begin();
    std::set<std::string> visited;
    std::vector<std::string> order; // newest first, merges excluded
    size_t remaining = newIds.size();
    bool anchorFound = false;
    walk([&](const std::string& commitId, bool merge) {
        if (merge) {
            // A batch of merges alone can still reorder commits sharing a timestamp.
            if (newIds.erase(commitId))
                --remaining;
            return false;
        }
        visited.emplace(commitId);
        if (newIds.erase(commitId))
            --remaining;
        order.emplace_back(commitId);
        if (remaining != 0)
            return false;
        while (oldIt != list.end() && visited.count((*oldIt)->id))
            ++oldIt;
        anchorFound = oldIt == list.end() || (*oldIt)->id == commitId;
        return anchorFound;
    });
    if (remaining != 0)
        return; // Should not happen: announced commits are always reachable from HEAD
    std::string anchorId;
    if (anchorFound) {
        anchorId = std::move(order.back());
        order.pop_back();
    }

    // Only displayed messages are part of the linear chain; reactions and edits are not.
    auto isChained = [](const libjami::SwarmMessage& m) {
        auto reactIt = m.body.find(CommitKey::REACT_TO);
        auto editIt = m.body.find(CommitKey::EDIT);
        return (reactIt == m.body.end() || reactIt->second.empty())
               && (editIt == m.body.end() || editIt->second.empty());
    };
    std::vector<Message> chain; // oldest first
    chain.reserve(order.size());
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        auto q = quickAccess.find(*it);
        if (q != quickAccess.end() && isChained(*q->second))
            chain.emplace_back(q->second);
    }
    if (chain.empty())
        return;

    // These messages are the newest ones in the history: detach them from the front
    // and put them back in log order.
    std::set<const libjami::SwarmMessage*> detaching;
    for (const auto& msg : chain)
        detaching.emplace(msg.get());
    for (auto it = list.begin(); it != list.end() && !detaching.empty();) {
        if (detaching.erase(it->get()))
            it = list.erase(it);
        else
            ++it;
    }
    for (const auto& msg : chain)
        list.emplace_front(msg);

    // Announce the chain from the first message whose predecessor changed, oldest
    // first: a client that places a message right after its parent needs the whole
    // tail replayed, since a message may keep its parent while that parent moves.
    std::vector<libjami::SwarmMessage> moved;
    const std::string* parentId = &anchorId;
    for (const auto& msg : chain) {
        if (!moved.empty() || msg->linearizedParent != *parentId) {
            msg->linearizedParent = *parentId;
            moved.emplace_back(*msg);
        }
        parentId = &msg->id;
    }
    for (const auto& msg : moved)
        emitSignal<libjami::ConversationSignal::SwarmMessageUpdated>(accountId, id, msg);
}

} // namespace jami
