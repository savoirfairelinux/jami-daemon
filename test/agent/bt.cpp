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

#include "agent/bt.h"

namespace BT {

std::map<std::string, std::function<bool(void)>> registered_behaviors;

std::unique_ptr<Node>
from_yaml(YAML::Node behavior)
{
    std::unique_ptr<Node> node;

    if (behavior.IsSequence()) {
        auto tmp = new Sequence();

        for (const auto& sub_behavior : behavior) {
            tmp->add(from_yaml(sub_behavior));
        }

        node.reset(dynamic_cast<Node*>(tmp));

    } else if (behavior.IsMap()) {
        auto tmp = new Selector();

        for (const auto& kv : behavior) {
            assert(kv.second.IsSequence());
            for (const auto& sub_behavior : kv.second) {
                tmp->add(from_yaml(sub_behavior));
            }
        }

        node.reset(dynamic_cast<Node*>(tmp));

    } else {
        node = std::make_unique<Execute>(behavior.as<std::string>());
    }

    return node;
}

}; // namespace BT
