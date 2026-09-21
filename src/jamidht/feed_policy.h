/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace jami {

inline constexpr char MIME_TYPE_FEED[] = "application/jami-feed+json";

struct FeedPolicy
{
    bool replies {false};
    bool closed {false};
    std::set<std::string> authorized;

    static std::optional<std::string> owner(const std::vector<std::string>& members)
    {
        if (members.size() != 1 || !validUri(members.front()))
            return {};
        return members.front();
    }

    static FeedPolicy merge(const FeedPolicy& base, const FeedPolicy& left, const FeedPolicy& right)
    {
        FeedPolicy result;
        result.closed = base.closed || left.closed || right.closed;
        result.replies = base.replies ? left.replies && right.replies : left.replies || right.replies;
        for (const auto& uri : left.authorized)
            if (!base.authorized.count(uri) || right.authorized.count(uri))
                result.authorized.insert(uri);
        for (const auto& uri : right.authorized)
            if (!base.authorized.count(uri) || left.authorized.count(uri))
                result.authorized.insert(uri);
        return result;
    }

    static bool validUri(std::string_view uri)
    {
        return uri.size() == 40 && std::all_of(uri.begin(), uri.end(), [](char c) {
                   return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
               });
    }

    static std::optional<FeedPolicy> fromInfos(const std::map<std::string, std::string>& infos)
    {
        FeedPolicy policy;
        auto flag = [&](const char* name, bool& out) {
            auto it = infos.find(name);
            if (it == infos.end())
                return true;
            if (it->second != "true" && it->second != "false")
                return false;
            out = it->second == "true";
            return true;
        };
        if (!flag("feedReplies", policy.replies) || !flag("feedClosed", policy.closed))
            return {};
        auto it = infos.find("feedAccess");
        if (it == infos.end() || it->second.empty())
            return policy;
        if (it->second.size() > 4096 * 41)
            return {};
        std::string_view access = it->second;
        while (!access.empty()) {
            const auto comma = access.find(',');
            const auto uri = access.substr(0, comma);
            if (!validUri(uri) || !policy.authorized.emplace(uri).second)
                return {};
            if (comma == std::string_view::npos)
                break;
            access.remove_prefix(comma + 1);
            if (access.empty())
                return {};
        }
        return policy;
    }

    std::string accessList() const
    {
        std::string result;
        for (const auto& uri : authorized) {
            if (!result.empty())
                result += ',';
            result += uri;
        }
        return result;
    }
};

} // namespace jami
