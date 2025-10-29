/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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

#include <string>
#include <string_view>
#include <map>
#include <filesystem>

namespace jami {
namespace vCard {

struct Delimiter
{
    constexpr static std::string_view SEPARATOR_TOKEN = ";";
    constexpr static std::string_view END_LINE_TOKEN = "\n";
    constexpr static std::string_view BEGIN_TOKEN = "BEGIN:VCARD";
    constexpr static std::string_view END_TOKEN = "END:VCARD";
    constexpr static std::string_view BEGIN_TOKEN_KEY = "BEGIN";
    constexpr static std::string_view END_TOKEN_KEY = "END";
};
;

struct Property
{
    constexpr static std::string_view UID = "UID";
    constexpr static std::string_view VCARD_VERSION = "VERSION";
    constexpr static std::string_view ADDRESS = "ADR";
    constexpr static std::string_view AGENT = "AGENT";
    constexpr static std::string_view BIRTHDAY = "BDAY";
    constexpr static std::string_view CATEGORIES = "CATEGORIES";
    constexpr static std::string_view CLASS = "CLASS";
    constexpr static std::string_view DELIVERY_LABEL = "LABEL";
    constexpr static std::string_view EMAIL = "EMAIL";
    constexpr static std::string_view FORMATTED_NAME = "FN";
    constexpr static std::string_view GEOGRAPHIC_POSITION = "GEO";
    constexpr static std::string_view KEY = "KEY";
    constexpr static std::string_view LOGO = "LOGO";
    constexpr static std::string_view MAILER = "MAILER";
    constexpr static std::string_view NAME = "N";
    constexpr static std::string_view NICKNAME = "NICKNAME";
    constexpr static std::string_view DESCRIPTION = "DESCRIPTION";
    constexpr static std::string_view NOTE = "NOTE";
    constexpr static std::string_view ORGANIZATION = "ORG";
    constexpr static std::string_view PHOTO = "PHOTO";
    constexpr static std::string_view PRODUCT_IDENTIFIER = "PRODID";
    constexpr static std::string_view REVISION = "REV";
    constexpr static std::string_view ROLE = "ROLE";
    constexpr static std::string_view SORT_STRING = "SORT-STRING";
    constexpr static std::string_view SOUND = "SOUND";
    constexpr static std::string_view TELEPHONE = "TEL";
    constexpr static std::string_view TIME_ZONE = "TZ";
    constexpr static std::string_view TITLE = "TITLE";
    constexpr static std::string_view RDV_ACCOUNT = "RDV_ACCOUNT";
    constexpr static std::string_view RDV_DEVICE = "RDV_DEVICE";
    constexpr static std::string_view URL = "URL";
    constexpr static std::string_view BASE64 = "ENCODING=BASE64";
    constexpr static std::string_view TYPE_PNG = "TYPE=PNG";
    constexpr static std::string_view TYPE_JPEG = "TYPE=JPEG";
    constexpr static std::string_view PHOTO_PNG = "PHOTO;ENCODING=BASE64;TYPE=PNG";
    constexpr static std::string_view PHOTO_JPEG = "PHOTO;ENCODING=BASE64;TYPE=JPEG";
};

struct Value
{
    constexpr static std::string_view TITLE = "title";
    constexpr static std::string_view DESCRIPTION = "description";
    constexpr static std::string_view AVATAR = "avatar";
    constexpr static std::string_view RDV_ACCOUNT = "rdvAccount";
    constexpr static std::string_view RDV_DEVICE = "rdvDevice";
};

namespace utils {

using VCardData = std::map<std::string, std::string, std::less<>>;

/**
 * Payload to vCard
 * @param content payload
 * @return the vCard representation
 */
VCardData toMap(std::string_view content);
VCardData initVcard();
std::string toString(const VCardData& vCard);
void removeByKey(VCardData& vCard, std::string_view key);
void save(const VCardData& vCard, const std::filesystem::path& path, const std::filesystem::path& pathLink);

} // namespace utils

} // namespace vCard
} // namespace jami
