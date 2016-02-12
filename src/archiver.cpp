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
    Json::FastWriter fastWriter;
    std::string output = fastWriter.write(root);

    // Compress
    std::string compressed;
    try {
        compressed = compress_string(output);
    } catch (const std::runtime_error& ex) {
        RING_ERR("Export failed: %s", ex.what());
        return;
    }
    // Encrypt

    // Write
    file_id << compressed;
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
    std::ifstream inStream(archivePath);
    std::stringstream buffer;
    buffer << inStream.rdbuf();

    // Decrypt

    // Decompress
    std::string uncompressed;
    try {
        uncompressed = decompress_string(buffer.str());
    } catch (const std::runtime_error& ex) {
        RING_ERR("Import failed: %s", ex.what());
        return;
    }

    // Add
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(uncompressed.c_str(),root)) {
        RING_ERR("Failed to parse %s", reader.getFormattedErrorMessages().c_str());
        return;
    }

    for(int i = 0 ; i < root.size(); i++) {
        jsonValueToAccount(root["accounts"][i]);
    }

}

std::map<std::string, std::string>
Archiver::jsonValueToAccount(Json::Value& value) {
    auto detailsMap = DRing::getAccountTemplate(value[DRing::Account::ConfProperties::TYPE].asString());
    for( Json::ValueIterator itr = value.begin() ; itr != value.end() ; itr++ ) {
        detailsMap[itr.key().asString()] = itr->asString();
        RING_DBG("import: %s %s", itr.key().asString().c_str(), itr->asString().c_str());
    }
    return detailsMap;
}

std::string
Archiver::compress_string(const std::string& str, int compressionlevel)
{
    z_stream zs; // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));

    if (deflateInit(&zs, compressionlevel) != Z_OK)
        throw(std::runtime_error("deflateInit failed while compressing."));

    zs.next_in = (Bytef*)str.data();
    zs.avail_in = str.size(); // set the z_stream's input

    int ret;
    char outbuffer[32768];
    std::string outstring;

    // retrieve the compressed bytes blockwise
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = deflate(&zs, Z_FINISH);

        if (outstring.size() < zs.total_out) {
            // append the block to the output string
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);

    deflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        // an error occurred that was not EOF
        std::ostringstream oss;
        oss << "Exception during zlib compression: (" << ret << ") " << zs.msg;
        throw(std::runtime_error(oss.str()));
    }

    return outstring;
}

std::string
Archiver::decompress_string(const std::string& str)
{
    z_stream zs; // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK)
        throw(std::runtime_error("inflateInit failed while decompressing."));

    zs.next_in = (Bytef*)str.data();
    zs.avail_in = str.size();

    int ret;
    char outbuffer[32768];
    std::string outstring;

    // get the decompressed bytes blockwise using repeated calls to inflate
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = inflate(&zs, 0);

        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }

    } while (ret == Z_OK);

    inflateEnd(&zs);

    // an error occurred that was not EOF
    if (ret != Z_STREAM_END) {
        std::ostringstream oss;
        oss << "Exception during zlib decompression: (" << ret << ") " << zs.msg;
        throw(std::runtime_error(oss.str()));
    }

    return outstring;
}

} // namespace ring
