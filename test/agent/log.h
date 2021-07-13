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

#include <fstream>
#include <memory>
#include <string>
#include <vector>

class LogHandler
{
protected:
    std::string context_;

public:
    LogHandler(const std::string& context)
        : context_(context)
    {}
    virtual ~LogHandler() = default;

    virtual void pushMessage(const std::string& message) = 0;
    virtual void flush() = 0;
};

class FileHandler : public LogHandler
{
    std::ofstream out_;
    std::vector<std::string> messages_;

public:
    FileHandler(const std::string& context, const std::string& to);

    virtual ~FileHandler();

    virtual void pushMessage(const std::string& message) override;

    virtual void flush() override;
};

class NetHandler : public LogHandler
{
    int sockfd_;
    std::vector<std::string> messages_;

public:
    NetHandler(const std::string& context, const std::string& to, uint16_t port);

    virtual ~NetHandler();

    virtual void pushMessage(const std::string& message) override;

    virtual void flush() override;
};

template<typename T, typename... Args>
std::unique_ptr<LogHandler>
makeLogHandler(Args... args)
{
    return std::make_unique<T>(args...);
}
