/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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
#include "jamidht/jamiaccount.h"
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

    // Conversations
    std::map<std::string, ConvInfo> conversations;
    std::map<std::string, ConversationRequest> conversationsRequests;

    /** Account configuration */
    std::map<std::string, std::string> config;

    /** Salt for the archive encryption password.  */
    std::vector<uint8_t> password_salt;

    AccountArchive() = default;
    AccountArchive(const std::vector<uint8_t>& data, const std::vector<uint8_t>& password_salt = {}) { deserialize(data, password_salt); }
    AccountArchive(const std::filesystem::path& path, std::string_view scheme = {}, const std::string& pwd = {}) { load(path, scheme, pwd); }

    /** Serialize structured archive data to memory. */
    std::string serialize() const;

    /** Deserialize archive from memory. */
    void deserialize(const std::vector<uint8_t>& data, const std::vector<uint8_t>& salt);

    /** Load archive from file, optionally encrypted with provided password. */
    void load(const std::filesystem::path& path, std::string_view scheme, const std::string& pwd) {
        auto data = fileutils::readArchive(path, scheme, pwd);
        deserialize(data.data, data.salt);
    }

    /** Save archive to file, optionally encrypted with provided password. */
    void save(const std::filesystem::path& path, std::string_view scheme, const std::string& password) const {
        fileutils::writeArchive(serialize(), path, scheme, password, password_salt);
    }
};

} // namespace jami
