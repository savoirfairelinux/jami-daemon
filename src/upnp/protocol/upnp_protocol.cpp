/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *	Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

#include "upnp_protocol.h"

namespace jami { namespace upnp {

void
UPnPProtocol::setOnIgdChanged(IgdListChangedCallback&& cb)
{
	updateIgdListCb_ = std::move(cb);
}

size_t
UPnPProtocol::addIGDListener(IgdFoundCallback&& cb)
{
	JAMI_DBG("UPnP Protocol: Adding IGD listener.");

	std::lock_guard<std::mutex> lock(validIGDMutex_);
	auto token = ++listenerToken_;
	igdListeners_.emplace(token, std::move(cb));

	return token;
}

void
UPnPProtocol::removeIGDListener(size_t token)
{
	std::lock_guard<std::mutex> lock(validIGDMutex_);
	if (igdListeners_.erase(token) > 0) {
		JAMI_DBG("PUPnP: Removing igd listener.");
	}
}


}} // namespace jami::upnp
