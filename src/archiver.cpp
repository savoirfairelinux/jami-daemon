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

#include <fstream>
#include <sys/stat.h>

#include "client/ring_signal.h"
#include "account_const.h"
#include "configurationmanager_interface.h"

#include "manager.h"
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

    // Add
    std::ofstream file_id;
    file_id.open(toDir + DIR_SEPARATOR_STR + "test.ring");
    Json::Value root;
    Json::Value array;

    for (size_t i = 0; i < accountIDs.size(); ++i) {
        auto detailsMap = Manager::instance().getAccountDetails(accountIDs[i]);
        if (detailsMap.empty()) {
            RING_WARN("Can't export account %s", accountIDs[i].c_str());
            continue;
        }

        auto jsonAccount = accountToJsonValue(detailsMap);
        array.append(jsonAccount);
    }
    root["accounts"] = array;

    // Compress

    // Encrypt

    // Write
    Json::StyledWriter styledWriter;
    file_id << styledWriter.write(root);
    file_id.close();
}

Json::Value
Archiver::accountToJsonValue(std::map<std::string, std::string> details) {
    Json::Value root;
    std::map<std::string, std::string>::iterator iter;
    for (iter = details.begin(); iter != details.end(); ++iter) {

        if (iter->first.compare(DRing::Account::ConfProperties::Ringtone::PATH) == 0) {
            // Ringtone path is not exportable
        } else if (iter->first.compare(DRing::Account::ConfProperties::TLS::CA_LIST_FILE) == 0 ||
                iter->first.compare(DRing::Account::ConfProperties::TLS::CERTIFICATE_FILE) == 0 ||
                iter->first.compare(DRing::Account::ConfProperties::TLS::PRIVATE_KEY_FILE) == 0) {
            // replace paths by the files content
            std::ifstream ifs(iter->second);
            std::string fileContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            root[iter->first] = fileContent;

        } else
            root[iter->first] = iter->second;
    }

    return root;
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
        Json::Value root;
        std::ifstream inStream(archivePath, std::ifstream::binary);
        inStream >> root;
        for( Json::ValueIterator itr = root.begin() ; itr != root.end() ; itr++ ) {
            jsonValueToAccount(*itr);
        }
    } catch (const std::runtime_error& ex) {
        RING_ERR("Import failed: %s", ex.what());
        return;
    }

    // Decrypt

    // Decompress

    // Add

}

std::map<std::string, std::string>
Archiver::jsonValueToAccount(Json::Value value) {

    auto detailsMap = DRing::getAccountTemplate(value[DRing::Account::ConfProperties::TYPE].asString());
    for( Json::ValueIterator itr = value.begin() ; itr != value.end() ; itr++ ) {
        detailsMap[itr.key().asString()] = itr->asString();
    }
}

} // namespace ring
