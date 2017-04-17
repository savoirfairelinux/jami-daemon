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

#pragma once

#include <vector>
#include <map>
#include <memory>
#include <string>
#include <cstdint>

#include "dring.h"
#include "dring/security_const.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace ring { namespace tls {

std::map<std::string, std::string> validateCertificate(const std::string& accountId, const std::string& certificate);
std::map<std::string, std::string> validateCertificatePath(const std::string& accountId, const std::string& certificatePath, const std::string& privateKey, const std::string& privateKeyPassword, const std::string& caList);

std::map<std::string, std::string> getCertificateDetails(const std::string& certificate);
std::map<std::string, std::string> getCertificateDetailsPath(const std::string& certificatePath, const std::string& privateKey, const std::string& privateKeyPassword);

std::vector<std::string> getPinnedCertificates();

std::vector<std::string> pinCertificate(const std::vector<uint8_t>& certificate, bool local);
bool unpinCertificate(const std::string& certId);

void pinCertificatePath(const std::string& path);
unsigned unpinCertificatePath(const std::string& path);

bool pinRemoteCertificate(const std::string& accountId, const std::string& certId);
bool setCertificateStatus(const std::string& account, const std::string& certId, const std::string& status);
std::vector<std::string> getCertificatesByStatus(const std::string& account, const std::string& status);

}} // namespace ring::tls
