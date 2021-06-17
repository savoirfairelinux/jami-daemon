/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion@savoirfairelinux.com>
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

#include <cassert>
#include <functional>
#include <map>

#include <yaml-cpp/yaml.h>

/* Jami */
#include "logger.h"

namespace BT {

extern std::map<std::string, std::function<bool(void)>> registered_behaviors;

static inline void
register_behavior(const std::string& name, std::function<bool(void)>&& behavior)
{
    registered_behaviors[name] = std::move(behavior);
}

class Node
{
public:
    virtual bool operator()(void) = 0;

    virtual ~Node() = default;
};

class Execute : public Node
{
    std::function<bool(void)> todo_ {};

public:
    Execute(const std::string& behavior_name)
    {
        try {
            todo_ = registered_behaviors.at(behavior_name);
        } catch (const std::exception& E) {
            JAMI_ERR("AGENT: Invalid behavior `%s`: %s", behavior_name.c_str(), E.what());
        }
    }

    virtual bool operator()(void) override { return todo_(); }
};

struct Sequence : public Node
{
    std::vector<std::unique_ptr<Node>> nodes_;

public:
    virtual bool operator()(void) override
    {
        for (const auto& node : nodes_) {
            if (not(*node)()) {
                return false;
            }
        }
        return true;
    }

    void add(std::unique_ptr<Node> node) { nodes_.push_back(std::move(node)); }
};

struct Selector : public Node
{
    std::vector<std::unique_ptr<Node>> nodes_;

public:
    virtual bool operator()(void) override
    {
        for (const auto& node : nodes_) {
            if ((*node)()) {
                return true;
            }
        }
        return false;
    }

    void add(std::unique_ptr<Node> node) { nodes_.push_back(std::move(node)); }
};

extern std::unique_ptr<Node> from_yaml(YAML::Node behavior);

}; // namespace BT
