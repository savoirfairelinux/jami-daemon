/*
 *  Copyright (C) 2004-2017 Savoir-faire Linux Inc.
 *
 *  Author: Groupe 7
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "securitymanager_interface.h"
#include "manager.h"
#include "tlsvalidator.h"
#include "certstore.h"
#include "logger.h"
#include "fileutils.h"
#include "archiver.h"
#include "ip_utils.h"
#include "ringdht/ringaccount.h"
#include "client/ring_signal.h"
#include "upnp/upnp_context.h"

#ifdef RING_UWP
#include "windirent.h"
#else
#include <dirent.h>
#endif

#include <cerrno>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#undef interface
#endif

namespace ring { namespace tls {

using ring::RingAccount;
using ring::tls::TlsValidator;
using ring::tls::CertificateStore;

std::map<std::string, std::string>
validateCertificate(const std::string&,
                    const std::string& certificate)
{
    try {
        return TlsValidator{CertificateStore::instance().getCertificate(certificate)}.getSerializedChecks();
    } catch(const std::runtime_error& e) {
        RING_WARN("Certificate loading failed: %s", e.what());
        return {{DRing::Certificate::ChecksNames::EXIST, DRing::Certificate::CheckValuesNames::FAILED}};
    }
}

std::map<std::string, std::string>
validateCertificatePath(const std::string&,
                    const std::string& certificate,
                    const std::string& privateKey,
                    const std::string& privateKeyPass,
                    const std::string& caList)
{
    try {
        return TlsValidator{certificate, privateKey, privateKeyPass, caList}.getSerializedChecks();
    } catch(const std::runtime_error& e) {
        RING_WARN("Certificate loading failed: %s", e.what());
        return {{DRing::Certificate::ChecksNames::EXIST, DRing::Certificate::CheckValuesNames::FAILED}};
    }
}

std::map<std::string, std::string>
getCertificateDetails(const std::string& certificate)
{
    try {
        return TlsValidator{CertificateStore::instance().getCertificate(certificate)}.getSerializedDetails();
    } catch(const std::runtime_error& e) {
        RING_WARN("Certificate loading failed: %s", e.what());
    }
    return {};
}

std::map<std::string, std::string>
getCertificateDetailsPath(const std::string& certificate, const std::string& privateKey, const std::string& privateKeyPassword)
{
    try {
        auto crt = std::make_shared<dht::crypto::Certificate>(ring::fileutils::loadFile(certificate));
        TlsValidator validator {certificate, privateKey, privateKeyPassword};
        CertificateStore::instance().pinCertificate(validator.getCertificate(), false);
        return validator.getSerializedDetails();
    } catch(const std::runtime_error& e) {
        RING_WARN("Certificate loading failed: %s", e.what());
    }
    return {};
}

std::vector<std::string>
getPinnedCertificates()
{
    return ring::tls::CertificateStore::instance().getPinnedCertificates();
}

std::vector<std::string>
pinCertificate(const std::vector<uint8_t>& certificate, bool local)
{
    return ring::tls::CertificateStore::instance().pinCertificate(certificate, local);
}

void
pinCertificatePath(const std::string& path)
{
    ring::tls::CertificateStore::instance().pinCertificatePath(path);
}

bool
unpinCertificate(const std::string& certId)
{
    return ring::tls::CertificateStore::instance().unpinCertificate(certId);
}

unsigned
unpinCertificatePath(const std::string& path)
{
    return ring::tls::CertificateStore::instance().unpinCertificatePath(path);
}

bool
pinRemoteCertificate(const std::string& accountId, const std::string& certId)
{
    if (auto acc = ring::Manager::instance().getAccount<ring::RingAccount>(accountId))
        return acc->findCertificate(certId);
    return false;
}

bool
setCertificateStatus(const std::string& accountId, const std::string& certId, const std::string& ststr)
{
    try {
        if (accountId.empty()) {
            ring::tls::CertificateStore::instance().setTrustedCertificate(certId, ring::tls::trustStatusFromStr(ststr.c_str()));
        } else if (auto acc = ring::Manager::instance().getAccount<ring::RingAccount>(accountId)) {
            try {
                auto status = ring::tls::TrustStore::statusFromStr(ststr.c_str());
                return acc->setCertificateStatus(certId, status);
            } catch (const std::out_of_range&) {
                auto status = ring::tls::trustStatusFromStr(ststr.c_str());
                return acc->setCertificateStatus(certId, status);
            }
        }
    } catch (const std::out_of_range&) {}
    return false;
}

std::vector<std::string>
getCertificatesByStatus(const std::string& accountId, const std::string& ststr)
{
     auto status = ring::tls::TrustStore::statusFromStr(ststr.c_str());
    if (auto acc = ring::Manager::instance().getAccount<ring::RingAccount>(accountId))
        return acc->getCertificatesByStatus(status);
    return {};
}

}} // namespace ring::tls
