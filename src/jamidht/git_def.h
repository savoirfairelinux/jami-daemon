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

#include <memory>
#include <git2.h>

namespace jami {

struct GitPackBuilderDeleter
{
    inline void operator()(git_packbuilder* p) const { git_packbuilder_free(p); }
};
using GitPackBuilder = std::unique_ptr<git_packbuilder, GitPackBuilderDeleter>;

struct GitRepositoryDeleter
{
    inline void operator()(git_repository* p) const { git_repository_free(p); }
};
using GitRepository = std::unique_ptr<git_repository, GitRepositoryDeleter>;

struct GitRevWalkerDeleter
{
    inline void operator()(git_revwalk* p) const { git_revwalk_free(p); }
};
using GitRevWalker = std::unique_ptr<git_revwalk, GitRevWalkerDeleter>;

struct GitCommitDeleter
{
    inline void operator()(git_commit* p) const { git_commit_free(p); }
};
using GitCommit = std::unique_ptr<git_commit, GitCommitDeleter>;

struct GitAnnotatedCommitDeleter
{
    inline void operator()(git_annotated_commit* p) const { git_annotated_commit_free(p); }
};
using GitAnnotatedCommit = std::unique_ptr<git_annotated_commit, GitAnnotatedCommitDeleter>;

struct GitIndexDeleter
{
    inline void operator()(git_index* p) const { git_index_free(p); }
};
using GitIndex = std::unique_ptr<git_index, GitIndexDeleter>;

struct GitTreeDeleter
{
    inline void operator()(git_tree* p) const { git_tree_free(p); }
};
using GitTree = std::unique_ptr<git_tree, GitTreeDeleter>;

struct GitRemoteDeleter
{
    inline void operator()(git_remote* p) const { git_remote_free(p); }
};
using GitRemote = std::unique_ptr<git_remote, GitRemoteDeleter>;

struct GitReferenceDeleter
{
    inline void operator()(git_reference* p) const { git_reference_free(p); }
};
using GitReference = std::unique_ptr<git_reference, GitReferenceDeleter>;

struct GitSignatureDeleter
{
    inline void operator()(git_signature* p) const { git_signature_free(p); }
};
using GitSignature = std::unique_ptr<git_signature, GitSignatureDeleter>;

struct GitObjectDeleter
{
    inline void operator()(git_object* p) const { git_object_free(p); }
};
using GitObject = std::unique_ptr<git_object, GitObjectDeleter>;

struct GitDiffDeleter
{
    inline void operator()(git_diff* p) const { git_diff_free(p); }
};
using GitDiff = std::unique_ptr<git_diff, GitDiffDeleter>;

struct GitDiffStatsDeleter
{
    inline void operator()(git_diff_stats* p) const { git_diff_stats_free(p); }
};
using GitDiffStats = std::unique_ptr<git_diff_stats, GitDiffStatsDeleter>;

struct GitIndexConflictIteratorDeleter
{
    inline void operator()(git_index_conflict_iterator* p) const { git_index_conflict_iterator_free(p); }
};
using GitIndexConflictIterator = std::unique_ptr<git_index_conflict_iterator, GitIndexConflictIteratorDeleter>;

struct GitBufDeleter
{
    inline void operator()(git_buf* b) const { git_buf_dispose(b); }
};
using GitBuf = std::unique_ptr<git_buf, GitBufDeleter>;

} // namespace jami