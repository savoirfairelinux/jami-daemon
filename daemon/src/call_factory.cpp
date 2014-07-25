/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author : Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include <call_factory.h>

template <> bool
CallFactory::empty<Call>()
{
    for (const auto& item : callMaps_) {
        const auto& map = item.second;
        if (!map.empty())
            return false;
    }

    return true;
}

template <> std::shared_ptr<Call>
CallFactory::getCall<Call>(const std::string& id)
{
    for (const auto& item : callMaps_) {
        const auto& map = item.second;
        const auto& iter = map.find(id);
        if (iter != map.cend())
            return iter->second;
    }

    return nullptr;
}

template <> std::vector<std::shared_ptr<Call> >
CallFactory::getAllCalls<Call>()
{
    std::vector<std::shared_ptr<Call> > v;

    for (const auto& item : callMaps_) {
        const CallMap& map = item.second;
        for (const auto item2 : map)
            v.push_back(item2.second);
    }

    v.shrink_to_fit();
    return v;
}

void CallFactory::removeCall(Call& call) {
    const auto& id = call.getCallId();
    DEBUG("Removing call %s", id.c_str());
    const auto& account = call.getAccount();
    auto& map = callMaps_.at(account.getAccountType());
    map.erase(id);
    DEBUG("Remaining %u %s call(s)", map.size(), account.getAccountType());
}

void CallFactory::removeCall(const std::string& id) {
    if (auto call = getCall(id)) {
        removeCall(*call);
    } else
        ERROR("No call with ID %s", id.c_str());
}
