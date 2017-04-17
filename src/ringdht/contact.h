/*
*  Copyright (C) 2014-2017 Savoir-faire Linux Inc.
*  Author: Hana Ben Arab <hana.ben-arab@polymtl.ca>
*  Author: Abderrahmane Laribi <abderrahmane.laribi@polymtl.ca>
*  Author: Sonia Farrah <sonia.farrah@polymtl.ca>
*  Author: Ahmed Belhaouane <ahmed.belhaouane@polymtl.ca>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <json/json.h>

#include <unistd.h>

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <cctype>
#include <cstdarg>
#include <string>
#include <fstream>

#include "string_utils.h"
#include "logger.h"
#include "fileutils.h"
namespace ring {
	class Contact
	{
	public:
		/** Time of contact addition */
		time_t added{ 0 };

		/** Time of contact removal */
		time_t removed{ 0 };

		/** True if we got confirmation that this contact also added us */
		bool confirmed{ false };

		/** True if the contact is banned (if not active) */
		bool banned{ false };

		/** True if the contact is an active contact (not banned nor removed) */
		bool isActive() const { return added > removed; }
		bool isBanned() const { return not isActive() and banned; }

		Contact() = default;
		Contact(const Json::Value& json) {
			added = json["added"].asInt();
			removed = json["removed"].asInt();
			confirmed = json["confirmed"].asBool();
			banned = json["banned"].asBool();
		}

		/**
		* Update this contact using other known contact information,
		* return true if contact state was changed.
		*/
		bool update(const Contact& c) {
			const auto copy = *this;
			if (c.added > added) {
				added = c.added;
			}
			if (c.removed > removed) {
				removed = c.removed;
				banned = c.banned;
			}
			if (c.confirmed != confirmed) {
				confirmed = c.confirmed or confirmed;
			}
			return hasSameState(copy);
		}
		bool hasSameState(const Contact& other) const {
			return other.isActive() != isActive()
				or other.isBanned() != isBanned()
				or other.confirmed != confirmed;
		}

		Json::Value toJson() const {
			Json::Value json;
			json["added"] = Json::Int64(added);
			json["removed"] = Json::Int64(removed);
			json["confirmed"] = confirmed;
			json["banned"] = banned;
			return json;
		}

		MSGPACK_DEFINE_MAP(added, removed, confirmed, banned)

	};
}