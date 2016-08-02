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

#include "client/ring_signal.h"
#include "account_const.h"
#include "configurationmanager_interface.h"

#include "manager.h"
#include "fileutils.h"
#include "logger.h"

#include <opendht/crypto.h>

#include <fstream>
#include <sys/stat.h>

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

int
Archiver::exportAccounts(std::vector<std::string> accountIDs,
                        std::string filepath,
                        std::string password)
{
    if (filepath.empty() || !accountIDs.size()) {
        RING_ERR("Missing arguments");
        return EINVAL;
    }

    std::size_t found = filepath.find_last_of(DIR_SEPARATOR_STR);
    auto toDir = filepath.substr(0,found);
    auto filename = filepath.substr(found+1);

    if (!fileutils::isDirectory(toDir)) {
        RING_ERR("%s is not a directory", toDir.c_str());
        return ENOTDIR;
    }

    // Add
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
    std::vector<uint8_t> compressed;
    try {
        compressed = compress(output);
    } catch (const std::runtime_error& ex) {
        RING_ERR("Export failed: %s", ex.what());
        return 1;
    }

    // Encrypt using provided password
    auto encrypted = dht::crypto::aesEncrypt(compressed, password);

    // Write
    try {
        fileutils::saveFile(toDir + DIR_SEPARATOR_STR + filename, encrypted);
    } catch (const std::runtime_error& ex) {
        RING_ERR("Export failed: %s", ex.what());
        return EIO;
    }
    return 0;
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

int
Archiver::importAccounts(std::string archivePath, std::string password)
{
    if (archivePath.empty()) {
        RING_ERR("Missing arguments");
        return EINVAL;
    }

    // Read file
    std::vector<uint8_t> file;
    try {
        file = fileutils::loadFile(archivePath);
    } catch (const std::exception& ex) {
        RING_ERR("Read failed: %s", ex.what());
        return ENOENT;
    }

    // Decrypt
    try {
        file = dht::crypto::aesDecrypt(file, password);
    } catch (const std::exception& ex) {
        RING_ERR("Decryption failed: %s", ex.what());
        return EPERM;
    }

    // Decompress
    try {
        file = decompress(file);
    } catch (const std::exception& ex) {
        RING_ERR("Decompression failed: %s", ex.what());
        return ERANGE;
    }

    try {
        // Decode string
        std::string decoded {file.begin(), file.end()};

        // Add
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(decoded.c_str(),root)) {
            RING_ERR("Failed to parse %s", reader.getFormattedErrorMessages().c_str());
            return ERANGE;
        }

        auto& accounts = root["accounts"];
        for (int i = 0, n = accounts.size(); i < n; ++i) {
            // Generate a new account id
            auto accountId = ring::Manager::instance().getNewAccountId();
            auto details = jsonValueToAccount(accounts[i], accountId);
            ring::Manager::instance().addAccount(details, accountId);
        }
    } catch (const std::exception& ex) {
        RING_ERR("Import failed: %s", ex.what());
        return ERANGE;
    }
    return 0;
}

std::map<std::string, std::string>
Archiver::jsonValueToAccount(Json::Value& value, const std::string& accountId) {
    auto idPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId;
    fileutils::check_dir(idPath_.c_str(), 0700);
    auto detailsMap = DRing::getAccountTemplate(value[DRing::Account::ConfProperties::TYPE].asString());

    for( Json::ValueIterator itr = value.begin() ; itr != value.end() ; itr++ ) {
        if (itr->asString().empty())
            continue;
        if (itr.key().asString().compare(DRing::Account::ConfProperties::TLS::CA_LIST_FILE) == 0) {
            std::string fileContent(itr->asString());
            fileutils::saveFile(idPath_ + DIR_SEPARATOR_STR "ca.key", {fileContent.begin(), fileContent.end()}, 0600);

        } else if (itr.key().asString().compare(DRing::Account::ConfProperties::TLS::PRIVATE_KEY_FILE) == 0) {
            std::string fileContent(itr->asString());
            fileutils::saveFile(idPath_ + DIR_SEPARATOR_STR "dht.key", {fileContent.begin(), fileContent.end()}, 0600);

        } else if (itr.key().asString().compare(DRing::Account::ConfProperties::TLS::CERTIFICATE_FILE) == 0) {
            std::string fileContent(itr->asString());
            fileutils::saveFile(idPath_ + DIR_SEPARATOR_STR "dht.crt", {fileContent.begin(), fileContent.end()}, 0600);
        } else
            detailsMap[itr.key().asString()] = itr->asString();
    }

    return detailsMap;
}

std::vector<uint8_t>
Archiver::compress(const std::string& str, int compressionlevel)
{
    auto destSize = compressBound(str.size());
    std::vector<uint8_t> outbuffer(destSize);
    int ret = ::compress(reinterpret_cast<Bytef*>(outbuffer.data()), &destSize, (Bytef*)str.data(), str.size());
    outbuffer.resize(destSize);

    if (ret != Z_OK) {
        std::ostringstream oss;
        oss << "Exception during zlib compression: (" << ret << ") ";
        throw(std::runtime_error(oss.str()));
    }

    return outbuffer;
}

std::vector<uint8_t>
Archiver::decompress(const std::vector<uint8_t>& str)
{
    z_stream zs; // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK)
        throw(std::runtime_error("inflateInit failed while decompressing."));

    zs.next_in = (Bytef*)str.data();
    zs.avail_in = str.size();

    int ret;
    std::vector<uint8_t> out;

    // get the decompressed bytes blockwise using repeated calls to inflate
    do {
        std::array<uint8_t, 32768> outbuffer;
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer.data());
        zs.avail_out = outbuffer.size();

        ret = inflate(&zs, 0);
        if (ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
            break;

        if (out.size() < zs.total_out) {
            // append the block to the output string
            out.insert(out.end(), outbuffer.begin(), outbuffer.begin() + zs.total_out - out.size());
        }
    } while (ret == Z_OK);

    inflateEnd(&zs);

    // an error occurred that was not EOF
    if (ret != Z_STREAM_END) {
        std::ostringstream oss;
        oss << "Exception during zlib decompression: (" << ret << ") " << zs.msg;
        throw(std::runtime_error(oss.str()));
    }

    return out;
}

} // namespace ring
