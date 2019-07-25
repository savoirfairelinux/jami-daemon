/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
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

void
publish(const std::string& accountID, const bool& status, const std::string& note)
{
    DRing::publish(accountID, status, note);
}

void
answerServerRequest(const std::string& uri, const bool& flag)
{
    DRing::answerServerRequest(uri, flag);
}

void
subscribeBuddy(const std::string& accountID, const std::string& uri, const bool& flag)
{
    DRing::subscribeBuddy(accountID, uri, flag);
}

auto
getSubscriptions(const std::string& accountID) -> decltype(DRing::getSubscriptions(accountID))
{
    return DRing::getSubscriptions(accountID);
}

void
setSubscriptions(const std::string& accountID, const std::vector<std::string>& uris)
{
    DRing::setSubscriptions(accountID, uris);
}
