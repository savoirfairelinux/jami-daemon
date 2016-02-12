/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
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

#include <string>
#include <vector>
#include <map>
#include <zlib.h>

namespace Json {
class Value;
};

namespace ring {

/**
 * Archiver is used to generate/read encrypted archives
 */
class Archiver {
public:
    static Archiver& instance();

    Archiver();

    /**
     * Create a protected archive
     * @param accountIDs The accounts to exports
     * @param toDir The directory where to put the resulting archive
     * @param password The mandatory password to set on the archive
     */
    void exportAccounts(std::vector<std::string> accountIDs,
                        std::string toDir,
                        std::string password);

    /**
     * Read a protected archive
     * @param archivePath The path to the archive file
     * @param password The password to read the archive
     */
    void importAccounts(std::string archivePath, std::string password);

private:
    NON_COPYABLE(Archiver);

    Json::Value accountToJsonValue(std::map<std::string, std::string> details);
    std::map<std::string, std::string> jsonValueToAccount(Json::Value& value);

    /**
     * Compress a STL string using zlib with given compression level and return
     * the binary data.
     */
    std::string compress_string(const std::string& str, int compressionlevel = Z_BEST_COMPRESSION);

    /**
     * Decompress an STL string using zlib and return the original data.
     */
    std::string decompress_string(const std::string& str);
};

} // namespace ring
