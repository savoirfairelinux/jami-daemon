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

namespace ring {

class NameDirectory
{
public:
    NameDirectory() {}
    static NameDirectory& instance();

    enum Response{ found, notFound, error };

    using LookupCallback = std::function<void(const std::string& result, const Response& response)>;
    void addrLookup(const std::string& addr, LookupCallback cb);
    void nameLookup(const std::string& name, LookupCallback cb);

    void registerName(const std::string& addr, const std::string& name);

    void setServer(const std::string& server) {
        server_ = server;
    }
    const std::string& getServer() const {
        return server_;
    }

private:
    std::string server_ {"5.196.89.112:3000"};
    std::map<std::string, std::string> nameCache_;
    std::map<std::string, std::string> addrCache_;
};

}
