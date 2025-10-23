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
#include "vcard.h"
#include "string_utils.h"
#include "fileutils.h"

#include <fstream>
#include <filesystem>
#include <map>

namespace jami {
namespace vCard {

namespace utils {

VCardData
toMap(std::string_view content)
{
    VCardData vCard;

    std::string_view line;
    while (jami::getline(content, line)) {
        if (line.size()) {
            const auto dblptPos = line.find(':');
            if (dblptPos == std::string::npos)
                continue;
            vCard.emplace(line.substr(0, dblptPos), line.substr(dblptPos + 1));
        }
    }
    return vCard;
}

VCardData
initVcard()
{
    return {
        {std::string(Property::VCARD_VERSION), "2.1"},
        {std::string(Property::FORMATTED_NAME), ""},
        {std::string(Property::PHOTO_PNG), ""},
    };
}

std::string
toString(const VCardData& vCard)
{
    size_t estimatedSize = 0;
    for (const auto& [key, value] : vCard) {
        if (Delimiter::BEGIN_TOKEN_KEY == key || Delimiter::END_TOKEN_KEY == key)
            continue;
        estimatedSize += key.size() + value.size() + 2;
    }
    std::string result;
    result.reserve(estimatedSize + Delimiter::BEGIN_TOKEN.size() + Delimiter::END_LINE_TOKEN.size()
                   + Delimiter::END_TOKEN.size() + Delimiter::END_LINE_TOKEN.size());

    result += Delimiter::BEGIN_TOKEN;
    result += Delimiter::END_LINE_TOKEN;

    for (const auto& [key, value] : vCard) {
        if (Delimiter::BEGIN_TOKEN_KEY == key || Delimiter::END_TOKEN_KEY == key)
            continue;
        result += key + ':' + value + '\n';
    }

    result += Delimiter::END_TOKEN;
    result += Delimiter::END_LINE_TOKEN;

    return result;
}

void
save(const VCardData& vCard, const std::filesystem::path& path, const std::filesystem::path& pathLink)
{
    std::filesystem::path tmpPath = path + std::filesystem::path(".tmp");
    std::ofstream file(tmpPath);
    if (file.is_open()) {
        file << vCard::utils::toString(vCard);
        file.close();
        std::filesystem::rename(tmpPath, path);
        fileutils::createFileLink(pathLink, path, true);
    }
}

void
removeByKey(VCardData& vCard, std::string_view key)
{
    for (auto it = vCard.begin(); it != vCard.end();) {
        if (jami::starts_with(it->first, key)) {
            it = vCard.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace utils
} // namespace vCard
} // namespace jami
