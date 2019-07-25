/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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
Register(const int32_t& pid, const std::string& name)
{
    ++numberOfClients_;
    signal_numberOfClientsChanged_.emit(numberOfClients_);
}

void
Unregister(const int32_t& pid)
{
    if (numberOfClients_ == 0) {
        std::cerr << "DBusDaemon: Unregister() was called when there are no registered clients.";
    }

    --numberOfClients_;
    signal_numberOfClientsChanged_.emit(numberOfClients_);
}
