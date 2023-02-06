/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
 *
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#include "jamidht/jamiaccount.h"
#include <string>

namespace jami {

void addVote(std::shared_ptr<JamiAccount> account,
             const std::string& convId,
             const std::string& votedUri,
             const std::string& content);

void simulateRemoval(std::shared_ptr<JamiAccount> account,
                     const std::string& convId,
                     const std::string& votedUri);

void addFile(std::shared_ptr<JamiAccount> account,
             const std::string& convId,
             const std::string& relativePath,
             const std::string& content = "");

void addAll(std::shared_ptr<JamiAccount> account, const std::string& convId);

void commit(std::shared_ptr<JamiAccount> account, const std::string& convId, Json::Value& message);

std::string commitInRepo(const std::string& repoPath,
                         std::shared_ptr<JamiAccount> account,
                         const std::string& message);
} // namespace jami