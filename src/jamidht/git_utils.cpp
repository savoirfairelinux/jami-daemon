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
#include "git_utils.h"

#include <gnutls/gnutls.h>
#include "base64.h"

#include <ctime>

namespace jami::git {
namespace {

constexpr size_t MAX_ENCODED_SIGNATURE = 16 * 1024;

using GnutlsBuffer = std::unique_ptr<unsigned char, decltype(gnutls_free)>;

std::optional<std::string>
base64Encode(const std::vector<uint8_t>& bytes)
{
    if (bytes.empty())
        return std::nullopt;
    return base64::encode(bytes);
}

std::optional<std::vector<uint8_t>>
base64Decode(std::string_view encoded)
{
    if (encoded.empty() || encoded.size() > MAX_ENCODED_SIGNATURE)
        return std::nullopt;
    try {
        auto decoded = base64::decode(encoded);
        if (decoded.empty())
            return std::nullopt;
        return decoded;
    } catch (const base64::base64_exception&) {
        return std::nullopt;
    }
}

} // namespace

std::string
oidString(const git_oid* oid)
{
    char text[GIT_OID_HEXSZ + 1];
    git_oid_tostr(text, sizeof(text), oid);
    return text;
}

bool
objectWithin(git_repository* repo, const git_oid* oid, git_object_t type, size_t limit)
{
    git_odb* raw = nullptr;
    if (git_repository_odb(&raw, repo) < 0)
        return false;
    GitOdb odb(raw);
    size_t size = 0;
    git_object_t actual = GIT_OBJECT_INVALID;
    return git_odb_read_header(&size, &actual, odb.get(), oid) == 0 && actual == type && size <= limit;
}

GitCommit
lookupCommit(git_repository* repo, const git_oid* oid, size_t limit)
{
    git_commit* raw = nullptr;
    if ((limit != UNLIMITED && !objectWithin(repo, oid, GIT_OBJECT_COMMIT, limit))
        || git_commit_lookup(&raw, repo, oid) < 0)
        return {};
    return GitCommit(raw);
}

GitTree
lookupTree(git_repository* repo, const git_oid* oid, size_t limit)
{
    git_tree* raw = nullptr;
    if ((limit != UNLIMITED && !objectWithin(repo, oid, GIT_OBJECT_TREE, limit)) || git_tree_lookup(&raw, repo, oid) < 0)
        return {};
    return GitTree(raw);
}

std::optional<std::string>
readBlob(git_repository* repo,
         const git_tree* tree,
         const std::string& directory,
         const std::string& name,
         size_t directoryLimit,
         size_t blobLimit)
{
    const auto directoryEntry = git_tree_entry_byname(tree, directory.c_str());
    if (!directoryEntry || git_tree_entry_filemode(directoryEntry) != GIT_FILEMODE_TREE)
        return std::nullopt;
    const auto child = lookupTree(repo, git_tree_entry_id(directoryEntry), directoryLimit);
    if (!child)
        return std::nullopt;
    const auto entry = git_tree_entry_byname(child.get(), name.c_str());
    if (!entry || git_tree_entry_type(entry) != GIT_OBJECT_BLOB
        || (blobLimit != UNLIMITED && !objectWithin(repo, git_tree_entry_id(entry), GIT_OBJECT_BLOB, blobLimit)))
        return std::nullopt;
    git_blob* raw = nullptr;
    if (git_blob_lookup(&raw, repo, git_tree_entry_id(entry)) < 0)
        return std::nullopt;
    std::unique_ptr<git_blob, decltype(&git_blob_free)> blob(raw, git_blob_free);
    return std::string(static_cast<const char*>(git_blob_rawcontent(blob.get())), git_blob_rawsize(blob.get()));
}

GitSignature
makeSignature(const std::string& name, const std::string& email)
{
    git_signature* raw = nullptr;
    const auto now = std::time(nullptr);
    if (git_signature_new(&raw, name.c_str(), email.c_str(), now, 0) < 0
        && git_signature_new(&raw, email.c_str(), email.c_str(), now, 0) < 0)
        return {};
    return GitSignature(raw);
}

std::optional<git_oid>
createSignedCommit(git_repository* repo,
                   const git_signature* signature,
                   const git_tree* tree,
                   std::vector<const git_commit*> parents,
                   const std::string& message,
                   const dht::crypto::PrivateKey& key)
{
    git_buf buffer {};
    GitBuf guard(&buffer);
    // libgit2 1.8.0, 1.8.1 and 1.8.3 declare the parents as 'git_commit *const *'.
#if LIBGIT2_VER_MAJOR == 1 && LIBGIT2_VER_MINOR == 8 \
    && (LIBGIT2_VER_REVISION == 0 || LIBGIT2_VER_REVISION == 1 || LIBGIT2_VER_REVISION == 3)
    std::vector<git_commit*> parentList;
    for (const auto* parent : parents)
        parentList.push_back(const_cast<git_commit*>(parent));
#else
    auto& parentList = parents;
#endif
    if (git_commit_create_buffer(&buffer,
                                 repo,
                                 signature,
                                 signature,
                                 nullptr,
                                 message.c_str(),
                                 tree,
                                 parentList.size(),
                                 parentList.empty() ? nullptr : parentList.data())
        < 0)
        return std::nullopt;
    const auto signedText = base64Encode(key.sign(reinterpret_cast<const uint8_t*>(buffer.ptr), buffer.size));
    git_oid result {};
    if (!signedText || git_commit_create_with_signature(&result, repo, buffer.ptr, signedText->c_str(), "signature") < 0)
        return std::nullopt;
    return result;
}

std::optional<SignedCommit>
extractSignature(git_repository* repo, const git_oid* oid)
{
    git_buf signature {}, data {};
    GitBuf signatureGuard(&signature), dataGuard(&data);
    auto id = *oid;
    if (git_commit_extract_signature(&signature, &data, repo, &id, "signature") < 0)
        return std::nullopt;
    return SignedCommit {std::string(data.ptr, data.size), std::string(signature.ptr, signature.size)};
}

bool
checkSignature(const dht::crypto::PublicKey& key, std::string_view data, std::string_view encodedSignature)
{
    const auto decoded = base64Decode(encodedSignature);
    return decoded
           && key.checkSignature(reinterpret_cast<const uint8_t*>(data.data()),
                                 data.size(),
                                 decoded->data(),
                                 decoded->size());
}

} // namespace jami::git
