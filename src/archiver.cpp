/*
 *  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
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

#include "client/ring_signal.h"
#include "account_const.h"
#include "configurationmanager_interface.h"

#include "manager.h"
#include "fileutils.h"
#include "logger.h"

#include <opendht/crypto.h>
#include <json/json.h>
#include <zlib.h>

#include <sys/stat.h>
#include <fstream>

namespace jami {
namespace archiver {

std::map<std::string, std::string>
jsonValueToAccount(Json::Value& value, const std::string& accountId) {
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

Json::Value
accountToJsonValue(const std::map<std::string, std::string>& details) {
    Json::Value root;
    for (const auto& i : details) {
        if (i.first == DRing::Account::ConfProperties::Ringtone::PATH) {
            // Ringtone path is not exportable
        } else if (i.first == DRing::Account::ConfProperties::TLS::CA_LIST_FILE ||
                   i.first == DRing::Account::ConfProperties::TLS::CERTIFICATE_FILE ||
                   i.first == DRing::Account::ConfProperties::TLS::PRIVATE_KEY_FILE) {
            // replace paths by the files content
            std::ifstream ifs(i.second);
            std::string fileContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            root[i.first] = fileContent;
        } else
            root[i.first] = i.second;
    }

    return root;
}

int
exportAccounts(const std::vector<std::string>& accountIDs,
                        const std::string& filepath,
                        const std::string& password)
{
    if (filepath.empty() || !accountIDs.size()) {
        JAMI_ERR("Missing arguments");
        return EINVAL;
    }

    std::size_t found = filepath.find_last_of(DIR_SEPARATOR_STR);
    auto toDir = filepath.substr(0,found);
    auto filename = filepath.substr(found+1);

    if (!fileutils::isDirectory(toDir)) {
        JAMI_ERR("%s is not a directory", toDir.c_str());
        return ENOTDIR;
    }

    // Add
    Json::Value root;
    Json::Value array;

    for (size_t i = 0; i < accountIDs.size(); ++i) {
        auto detailsMap = Manager::instance().getAccountDetails(accountIDs[i]);
        if (detailsMap.empty()) {
            JAMI_WARN("Can't export account %s", accountIDs[i].c_str());
            continue;
        }

        auto jsonAccount = accountToJsonValue(detailsMap);
        array.append(jsonAccount);
    }
    root["accounts"] = array;
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    auto output = Json::writeString(wbuilder, root);

    // Compress
    std::vector<uint8_t> compressed;
    try {
        compressed = compress(output);
    } catch (const std::runtime_error& ex) {
        JAMI_ERR("Export failed: %s", ex.what());
        return 1;
    }

    // Encrypt using provided password
    auto encrypted = dht::crypto::aesEncrypt(compressed, password);

    // Write
    try {
        fileutils::saveFile(toDir + DIR_SEPARATOR_STR + filename, encrypted);
    } catch (const std::runtime_error& ex) {
        JAMI_ERR("Export failed: %s", ex.what());
        return EIO;
    }
    return 0;
}

int
importAccounts(std::string archivePath, std::string password)
{
    if (archivePath.empty()) {
        JAMI_ERR("Missing arguments");
        return EINVAL;
    }

    // Read file
    std::vector<uint8_t> file;
    try {
        file = fileutils::loadFile(archivePath);
    } catch (const std::exception& ex) {
        JAMI_ERR("Read failed: %s", ex.what());
        return ENOENT;
    }

    // Decrypt
    try {
        file = dht::crypto::aesDecrypt(file, password);
    } catch (const std::exception& ex) {
        JAMI_ERR("Decryption failed: %s", ex.what());
        return EPERM;
    }

    // Decompress
    try {
        file = decompress(file);
    } catch (const std::exception& ex) {
        JAMI_ERR("Decompression failed: %s", ex.what());
        return ERANGE;
    }

    try {
        const auto* char_file_begin = reinterpret_cast<const char*>(&file[0]);
        const auto* char_file_end = reinterpret_cast<const char*>(&file[file.size()]);

        // Add
        std::string err;
        Json::Value root;
        Json::CharReaderBuilder rbuilder;
        auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
        if (!reader->parse(char_file_begin, char_file_end, &root, &err)) {
            JAMI_ERR() << "Failed to parse " << err;
            return ERANGE;
        }

        auto& accounts = root["accounts"];
        for (int i = 0, n = accounts.size(); i < n; ++i) {
            // Generate a new account id
            auto accountId = jami::Manager::instance().getNewAccountId();
            auto details = jsonValueToAccount(accounts[i], accountId);
            jami::Manager::instance().addAccount(details, accountId);
        }
    } catch (const std::exception& ex) {
        JAMI_ERR("Import failed: %s", ex.what());
        return ERANGE;
    }
    return 0;
}

std::vector<uint8_t>
compress(const std::string& str)
{
    auto destSize = compressBound(str.size());
    std::vector<uint8_t> outbuffer(destSize);
    int ret = ::compress(reinterpret_cast<Bytef*>(outbuffer.data()), &destSize, (Bytef*)str.data(), str.size());
    outbuffer.resize(destSize);

    if (ret != Z_OK) {
        std::ostringstream oss;
        oss << "Exception during zlib compression: (" << ret << ") ";
        throw std::runtime_error(oss.str());
    }

    return outbuffer;
}

void
compressGzip(const std::string& str, const std::string& path)
{
    auto fi = gzopen(path.c_str(), "wb");
    gzwrite(fi, str.data(), str.size());
    gzclose(fi);
}

std::vector<uint8_t>
decompressGzip(const std::string& path)
{
    std::vector<uint8_t> out;
    auto fi = gzopen(path.c_str(),"rb");
    gzrewind(fi);
    while (not gzeof(fi)) {
        std::array<uint8_t, 32768> outbuffer;
        int len = gzread(fi, outbuffer.data(), outbuffer.size());
        if (len == -1) {
            gzclose(fi);
            throw std::runtime_error("Exception during gzip decompression");
        }
        out.insert(out.end(), outbuffer.begin(), outbuffer.begin() +  len);
    }
    gzclose(fi);
    return out;
}

std::vector<uint8_t>
decompress(const std::vector<uint8_t>& str)
{
    z_stream zs; // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK)
        throw std::runtime_error("inflateInit failed while decompressing.");

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

}} // namespace jami::archiver
