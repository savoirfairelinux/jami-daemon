/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *  Author : Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "jami_contact.h"
#include "fileutils.h"

#include <opendht/crypto.h>
#include <memory>
#include <vector>
#include <map>
#include <string>

namespace jami {

/**
 * Crypto material contained in the archive,
 * not persisted in the account configuration
 */
struct AccountArchive
{
    /** Account main private key and certificate chain */
    dht::crypto::Identity id;

    /** Generated CA key (for self-signed certificates) */
    std::shared_ptr<dht::crypto::PrivateKey> ca_key;

    /** Revoked devices */
    std::shared_ptr<dht::crypto::RevocationList> revoked;

    /** Ethereum private key */
    std::vector<uint8_t> eth_key;

    /** Contacts */
    std::map<dht::InfoHash, Contact> contacts;

    /** Account configuration */
    std::map<std::string, std::string> config;

    AccountArchive() = default;
    AccountArchive(const std::vector<uint8_t>& data) { deserialize(data); }
    AccountArchive(const std::string& path, const std::string& password) { load(path, password); }

    /** Serialize structured archive data to memory. */
    std::string serialize() const;

    /** Deserialize archive from memory. */
    void deserialize(const std::vector<uint8_t>& data);

    /** Load archive from file, optionally encrypted with provided password. */
    void load(const std::string& path, const std::string& password = {}) { deserialize(fileutils::readArchive(path, password)); }

    /** Save archive to file, optionally encrypted with provided password. */
    void save(const std::string& path, const std::string& password = {}) const { fileutils::writeArchive(serialize(), path, password); }
};

}
