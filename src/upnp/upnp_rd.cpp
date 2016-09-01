/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
 *
 *  Author: Caspar Cedro
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

#include "upnp_rd.h"

namespace ring { namespace upnp {

	std::vector<std::string> RingDevice::getRingAccounts(){
		int counter = 0;
		std::vector<std::string> temp;
		for(unsigned int i = 0; i < ringAccounts_.length(); i++){
			if(ringAccounts_.at(i) == '|'){
				counter++;
			} else {
				temp[counter] += ringAccounts_[i];
			}
		}
		return temp;
	}

}} // namespace ring::upnp
