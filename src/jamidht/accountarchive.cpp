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

#include "accountarchive.h"
#include "account_const.h"
#include "configurationmanager_interface.h"
#include "configkeys.h"
#include "base64.h"
#include "logger.h"

namespace jami {

void
AccountArchive::deserialize(const std::vector<uint8_t>& dat)
{
    JAMI_DBG("Loading account archive (%lu bytes)", dat.size());

    // Decode string
    auto* char_data = reinterpret_cast<const char*>(&dat[0]);
    std::string err;
    Json::Value value;
    Json::CharReaderBuilder rbuilder;
    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
    if (!reader->parse(char_data, char_data + dat.size(), &value, &err)) {
        JAMI_ERR() << "Archive JSON parsing error: " << err;
        throw std::runtime_error("failed to parse JSON");
    }

    // Import content
    try {
        config = DRing::getAccountTemplate(DRing::Account::ProtocolNames::RING);
        for (Json::ValueIterator itr = value.begin() ; itr != value.end() ; itr++) {
            try {
                const auto key = itr.key().asString();
                if (key.empty())
                    continue;
                if (key.compare(DRing::Account::ConfProperties::TLS::CA_LIST_FILE) == 0) {
                } else if (key.compare(DRing::Account::ConfProperties::TLS::PRIVATE_KEY_FILE) == 0) {
                } else if (key.compare(DRing::Account::ConfProperties::TLS::CERTIFICATE_FILE) == 0) {
                } else if (key.compare(Conf::RING_CA_KEY) == 0) {
                    ca_key = std::make_shared<dht::crypto::PrivateKey>(base64::decode(itr->asString()));
                } else if (key.compare(Conf::RING_ACCOUNT_KEY) == 0) {
                    id.first = std::make_shared<dht::crypto::PrivateKey>(base64::decode(itr->asString()));
                } else if (key.compare(Conf::RING_ACCOUNT_CERT) == 0) {
                    id.second = std::make_shared<dht::crypto::Certificate>(base64::decode(itr->asString()));
                } else if (key.compare(Conf::RING_ACCOUNT_CONTACTS) == 0) {
                    for (Json::ValueIterator citr = itr->begin() ; citr != itr->end() ; citr++) {
                        dht::InfoHash h {citr.key().asString()};
                        if (h != dht::InfoHash{})
                            contacts.emplace(h, Contact{*citr});
                    }
                } else if (key.compare(Conf::ETH_KEY) == 0) {
                    eth_key = base64::decode(itr->asString());
                } else if (key.compare(Conf::RING_ACCOUNT_CRL) == 0) {
                    revoked = std::make_shared<dht::crypto::RevocationList>(base64::decode(itr->asString()));
                } else
                    config[key] = itr->asString();
            } catch (const std::exception& ex) {
                JAMI_ERR("Can't parse JSON entry with value of type %d: %s", (unsigned)itr->type(), ex.what());
            }
        }
    } catch (const std::exception& ex) {
        JAMI_ERR("Can't parse JSON: %s", ex.what());
    }
}

std::string
AccountArchive::serialize() const
{
    Json::Value root;

    for (const auto& it : config)
        root[it.first] = it.second;

    if (ca_key and *ca_key)
        root[Conf::RING_CA_KEY] = base64::encode(ca_key->serialize());

    root[Conf::RING_ACCOUNT_KEY] = base64::encode(id.first->serialize());
    root[Conf::RING_ACCOUNT_CERT] = base64::encode(id.second->getPacked());
    root[Conf::ETH_KEY] = base64::encode(eth_key);

    if (revoked)
        root[Conf::RING_ACCOUNT_CRL] = base64::encode(revoked->getPacked());

    std::cout << "AccountArchive::serialize() 2" << std::endl;

    if (not contacts.empty()) {
        Json::Value& jsonContacts = root[Conf::RING_ACCOUNT_CONTACTS];
        for (const auto& c : contacts)
            jsonContacts[c.first.toString()] = c.second.toJson();
    }

    std::cout << "AccountArchive::serialize() 3" << std::endl;

    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    return Json::writeString(wbuilder, root);
}


}
