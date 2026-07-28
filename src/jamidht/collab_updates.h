/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
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

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace jami {
namespace collab {

// Updates live in commit messages, and git compresses those extremely well: a
// repository whose messages hold 400 MB of text packs into 92 KB when the lines
// repeat, and still only 135 KB when they are random base64 (both measured).
// The 256 MiB cap on a fetch therefore bounds what crosses the wire and almost
// nothing else — a pack of that size can expand into hundreds of gigabytes of
// message text, which the walk below would otherwise accumulate in memory. What
// follows bounds what is read back out, which is the part that has to fit.

// One update, base64-encoded. applyCollaborativeUpdate() refuses anything above
// 8 MiB decoded, so a longer line cannot be a legitimate update whatever it
// claims to be; base64 costs a third more than the bytes it carries.
constexpr size_t MAX_UPDATE_LINE {12 * 1024 * 1024};

// Updates bundled into a single checkpoint. A checkpoint is written on a pause
// in typing or every few seconds, and one keystroke is one update: four thousand
// is orders of magnitude past what a person produces between two of them.
constexpr size_t MAX_UPDATES_PER_COMMIT {4096};

// And over a whole history walk, so that a long chain of individually plausible
// checkpoints cannot add up to an implausible total either.
constexpr size_t MAX_COLLECTED_UPDATES {1 << 20};
constexpr size_t MAX_COLLECTED_BYTES {256 * 1024 * 1024};

constexpr std::string_view CHECKPOINT_PREFIX {"checkpoint: "};

/**
 * Append the CRDT updates recorded in one commit message to @c out.
 *
 * A dropped line is a bounded loss: the CRDT tolerates a missing update and
 * converges again on the next one, whereas an unbounded read does not recover.
 * So an oversized line is skipped and the rest of the message is still read,
 * while a message holding an implausible number of them is abandoned.
 *
 * @return false when the message was cut short by a limit.
 */
inline bool
appendMessageUpdates(std::string_view view, std::vector<std::string>& out)
{
    // Only checkpoints carry updates. Renames, merges and the initial commit
    // share the same history and must contribute nothing.
    if (view.substr(0, CHECKPOINT_PREFIX.size()) != CHECKPOINT_PREFIX)
        return true;
    // The subject is followed by a blank line, then one update per line.
    const auto body = view.find("\n\n");
    if (body == std::string_view::npos)
        return true;

    size_t pos = body + 2;
    size_t taken = 0;
    while (pos < view.size()) {
        auto end = view.find('\n', pos);
        if (end == std::string_view::npos)
            end = view.size();
        if (end > pos) {
            if (end - pos > MAX_UPDATE_LINE) {
                // Skipped, not fatal: the rest of the message may be sound.
            } else if (++taken > MAX_UPDATES_PER_COMMIT) {
                return false;
            } else {
                out.emplace_back(view.substr(pos, end - pos));
            }
        }
        pos = end + 1;
    }
    return true;
}

/**
 * Whether a walk that has already collected @c count updates totalling
 * @c bytes has to stop.
 *
 * Stopping mid-history loses work, but loses a bounded amount of it: what was
 * collected still applies, and the rest arrives on a later fetch. Growing
 * without limit is not bounded at all.
 */
inline bool
collectionIsExhausted(size_t count, size_t bytes)
{
    return count > MAX_COLLECTED_UPDATES || bytes > MAX_COLLECTED_BYTES;
}

} // namespace collab
} // namespace jami
