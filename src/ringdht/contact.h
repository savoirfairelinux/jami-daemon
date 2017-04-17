
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