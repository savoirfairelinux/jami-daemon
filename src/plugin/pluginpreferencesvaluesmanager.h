/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
#include "noncopyable.h"
#include "fileutils.h"

#include <string>
#include <map>

namespace jami {
class PluginPreferencesValuesManager
{
public:
    PluginPreferencesValuesManager() = delete;
    NON_COPYABLE(PluginPreferencesValuesManager);
    
    /**
     * @brief savePreferenceValue
     * @param preferencesValuesFilePath
     * @param key
     * @param value
     * @return true if success
     */
    static bool savePreferenceValue(const std::string& preferencesValuesFilePath,
                                    const std::string& key, const std::string& value);
    
    /**
     * @brief getPreferencesValuesMap
     * @param rootPath
     * @return map of preferences from the specified preferencesValuesFilePath 
     */
    static std::map<std::string, std::string> getPreferencesValuesMap(
        const std::string& preferencesValuesFilePath);
};
}


