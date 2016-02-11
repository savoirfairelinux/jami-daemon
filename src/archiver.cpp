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

#include "archiver.h"

#include <json/json.h>
#include <json/writer.h>

#include <fstream>
#include <sys/stat.h>

#include "client/ring_signal.h"

#include "fileutils.h"
#include "logger.h"

namespace ring {

Archiver&
Archiver::instance()
{
    // Meyers singleton
    static Archiver instance_;
    return instance_;
}

Archiver::Archiver()
{

}

void
Archiver::exportAccounts(std::vector<std::string> accountIDs,
                    std::string toDir,
                    std::string password)
{
    if (toDir.empty() || password.empty() || !accountIDs.size()) {
        RING_ERR("Missing arguments");
        return;
    }

    if (!fileutils::isDirectory(toDir)) {
        RING_ERR("%s is not a directory", toDir.c_str());
        return;
    }

    // Create
    std::ofstream file_id;
    file_id.open(toDir + DIR_SEPARATOR_STR + "test.ring");
    Json::Value fromScratch;
    Json::Value array;
    array.append("hello");
    array.append("world");
    fromScratch["hello"] = "world";
    fromScratch["number"] = 2;
    fromScratch["array"] = array;
    fromScratch["object"]["hello"] = "world";

    // Compress

    // Encrypt

    // Write
    Json::StyledWriter styledWriter;
    file_id << styledWriter.write(fromScratch);
    file_id.close();
}

void
Archiver::importAccounts(std::string archivePath, std::string password)
{
    if (archivePath.empty() || password.empty()) {
        RING_ERR("Missing arguments");
        return;
    }

    // Read
    try {
        fileutils::loadFile(archivePath);
    } catch (const std::runtime_error& ex) {
        RING_ERR("Import failed: %s", ex.what());
        return;
    }

    // Decrypt

    // Decompress

    // Read
}

} // namespace ring
