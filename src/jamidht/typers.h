/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

#include <asio.hpp>
#include <map>
#include <memory>
#include <set>
#include <string>

namespace jami {

class JamiAccount;

class Typers: public std::enable_shared_from_this<Typers>
{
public:
    Typers(const std::shared_ptr<JamiAccount>& acc, const std::string &convId);
    ~Typers();

    /**
     * Add typer to the list of typers
     * @param typer
     * @param sendMessage (should be true for local typer, false for remote typer)
     */
    void addTyper(const std::string &typer, bool sendMessage = false);
    void removeTyper(const std::string &typer, bool sendMessage = false);

private:
    void onTyperTimeout(const asio::error_code& ec, const std::string &typer);

    std::shared_ptr<asio::io_context> ioContext_;
    std::map<std::string, asio::steady_timer> watcher_;

    std::weak_ptr<JamiAccount> acc_;
    std::string accountId_;
    std::string convId_;
    std::string selfUri_;
};

} // namespace jami
