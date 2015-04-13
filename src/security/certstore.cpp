/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include "certstore.h"

#include "fileutils.h"

namespace ring {
namespace tls {

CertificateStore&
CertificateStore::instance() {
    // Meyers singleton
    static CertificateStore instance_;
    return instance_;
}


CertificateStore::CertificateStore()
: certPath_(fileutils::get_data_dir()+DIR_SEPARATOR_CH+"certificates"),
  bannedCertPath_(fileutils::get_data_dir()+DIR_SEPARATOR_CH+"certificates"+DIR_SEPARATOR_CH+"banned")
{
    fileutils::check_dir(certPath_.c_str());
    fileutils::check_dir(bannedCertPath_.c_str());
    loadBannedCertificates();
    loadCertificates();
}

void
CertificateStore::loadCertificates()
{
    auto dir_content = fileutils::readDirectory(certPath_);
    for (const auto& f : dir_content) {
        try {
            auto crt = crypto::Certificate(fileutils::loadFile(certPath_+DIR_SEPARATOR_CH+f));
            auto id = crt.getId().toString();
            if (id != f or isBanned(id))
                throw std::logic_error({});
            certs_.emplace(crt.getId().toString(), std::make_shared<crypto::Certificate>(std::move(crt)));
        } catch (const std::exception& e) {
            remove((certPath_+DIR_SEPARATOR_CH+f).c_str());
        }
    }
}

void
CertificateStore::loadBannedCertificates()
{
    auto dir_content = fileutils::readDirectory(bannedCertPath_);
    for (const auto& f : dir_content) {
        try {
            auto crt = crypto::Certificate(fileutils::loadFile(bannedCertPath_+DIR_SEPARATOR_CH+f));
            auto id = crt.getId().toString();
            if (id != f)
                continue;
            bannedCerts_.insert(id);
        } catch (const std::exception& e) {}
    }
}

std::vector<std::string>
CertificateStore::getCertificateList() const
{
    std::vector<std::string> certIds;
    for (const auto& crt : certs_)
        certIds.emplace_back(crt.first);
    return certIds;
}

std::shared_ptr<crypto::Certificate>
CertificateStore::getCertificate(const std::string& k) const
{
    auto cit = certs_.find(k);
    if (cit == certs_.cend()) {
        try {
            return std::make_shared<crypto::Certificate>(fileutils::loadFile(k));
        } catch (const std::exception& e) {
            return {};
        }
    }
    return cit->second;
}

std::string
CertificateStore::addCertificate(const std::vector<uint8_t>& crt)
{
    try {
        return addCertificate(crt);
    } catch (const std::exception& e) {}
    return {};
}

std::string
CertificateStore::addCertificate(crypto::Certificate&& crt)
{
    return addCertificate(std::make_shared<crypto::Certificate>(std::move(crt)));
}

std::string
CertificateStore::addCertificate(std::shared_ptr<crypto::Certificate> crt)
{
    const auto id = crt->getId().toString();
    if (isBanned(id))
        return {};
    const auto& it = *certs_.emplace(std::move(id), std::move(crt)).first;
    fileutils::saveFile(certPath_+DIR_SEPARATOR_CH+it.first, it.second->getPacked());
    return it.first;
}

bool
CertificateStore::removeCertificate(const std::string& id)
{
    certs_.erase(id);
    return remove((certPath_+DIR_SEPARATOR_CH+id).c_str()) == 0;
}

bool
CertificateStore::banCertificate(const std::string& id)
{
    bannedCerts_.emplace(id);
    certs_.erase(id);
    return rename((certPath_   +DIR_SEPARATOR_CH+id).c_str(),
                  (bannedCertPath_+DIR_SEPARATOR_CH+id).c_str()) == 0;
}

}} // namespace ring::tls
