/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <functional>
#include <map>
#include <string>

namespace ring {

class NameDirectory
{
public:
    NameDirectory() {}
    NameDirectory(const std::string& s) : server_(s) {}

    static NameDirectory& instance(const std::string& server);
    static NameDirectory& instance() { return instance(DEFAULT_SERVER); }

    enum class Response : int { found = 0, invalidName, notFound, error };
    enum class RegistrationResponse : int { success = 0, invalidName, alreadyTaken, error };

    using LookupCallback = std::function<void(const std::string& result, Response response)>;
    void lookupAddress(const std::string& addr, LookupCallback cb);
    void lookupName(const std::string& name, LookupCallback cb);

    using RegistrationCallback = std::function<void(RegistrationResponse response)>;
    void registerName(const std::string& addr, const std::string& name, const std::string& owner, RegistrationCallback cb);

    const std::string& getServer() const {
        return server_;
    }

private:
    constexpr static const char* const DEFAULT_SERVER = "5.196.89.112:3000";

    const std::string server_ {DEFAULT_SERVER};
    std::map<std::string, std::string> nameCache_;
    std::map<std::string, std::string> addrCache_;

    bool validateName(const std::string& name) const;

};

}
