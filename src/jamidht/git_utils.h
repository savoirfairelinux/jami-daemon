/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "git_def.h"

#include <opendht/crypto.h>

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Repository-neutral libgit2 helpers shared by conversations and circles.
namespace jami::git {

constexpr size_t UNLIMITED = std::numeric_limits<size_t>::max();

std::string oidString(const git_oid* oid);
// Reads only the object header, so oversized or mistyped objects are never parsed.
bool objectWithin(git_repository* repo, const git_oid* oid, git_object_t type, size_t limit);
GitCommit lookupCommit(git_repository* repo, const git_oid* oid, size_t limit = UNLIMITED);
GitTree lookupTree(git_repository* repo, const git_oid* oid, size_t limit = UNLIMITED);
// Reads <directory>/<name>; the limits bound the directory tree and the blob.
std::optional<std::string> readBlob(git_repository* repo,
                                    const git_tree* tree,
                                    const std::string& directory,
                                    const std::string& name,
                                    size_t directoryLimit = UNLIMITED,
                                    size_t blobLimit = UNLIMITED);

// Uses the email as name when libgit2 rejects the display name.
GitSignature makeSignature(const std::string& name, const std::string& email);
// Stores a commit signed by key in its "signature" header without moving any reference.
std::optional<git_oid> createSignedCommit(git_repository* repo,
                                          const git_signature* signature,
                                          const git_tree* tree,
                                          std::vector<const git_commit*> parents,
                                          const std::string& message,
                                          const dht::crypto::PrivateKey& key);

struct SignedCommit
{
    std::string data;
    std::string signature;
};
std::optional<SignedCommit> extractSignature(git_repository* repo, const git_oid* oid);
bool checkSignature(const dht::crypto::PublicKey& key, std::string_view data, std::string_view encodedSignature);

} // namespace jami::git
