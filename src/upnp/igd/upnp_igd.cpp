/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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

#include "upnp_igd.h"

namespace jami { namespace upnp {

#if HAVE_LIBUPNP

UPnPIGD::UPnPIGD(std::string&& UDN,std::string&& baseURL,std::string&& friendlyName,std::string&& serviceType,std::string&& serviceId,std::string&& controlURL,std::string&& eventSubURL): 
    UDN_(std::move(UDN)), 
    baseURL_(std::move(baseURL)), 
    friendlyName_(std::move(friendlyName)), 
    serviceType_(std::move(serviceType)), 
    serviceId_(std::move(serviceId)), 
    controlURL_(std::move(controlURL)), 
    eventSubURL_(std::move(eventSubURL))
{

}

#endif

}} // namespace jami::upnp