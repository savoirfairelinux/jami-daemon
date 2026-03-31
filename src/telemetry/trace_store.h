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

#include "trace_types.h"

#include <opentelemetry/sdk/trace/span_data.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace jami::telemetry::detail {

class TraceStore
{
public:
    static constexpr std::size_t kDefaultCapacityBytes = 5U * 1024U * 1024U;

    /**
     * @brief TraceStore Creates the bounded in-memory span ring buffer.
     * @param capacityBytes Maximum serialized size retained in memory.
     * @return void
     */
    explicit TraceStore(std::size_t capacityBytes = kDefaultCapacityBytes);

    /**
     * @brief clear Drops all buffered spans and resets sequence tracking.
     * @return void
     */
    void clear();
    /**
     * @brief appendCompletedSpan Snapshots a finished SDK span into the ring buffer.
     * @param spanData Completed SDK span data.
     * @return true when the span was buffered successfully.
     */
    bool appendCompletedSpan(const trace_sdk::SpanData& spanData);

    /**
     * @brief copySpans Copies the current buffered span snapshots.
     * @return Snapshot copy suitable for export without holding the store lock.
     */
    std::vector<SpanSnapshot> copySpans() const;
    /**
     * @brief pendingSpansSince Returns spans newer than a given upload sequence.
     * @param sequence Last sequence already consumed.
     * @param maxCount Maximum number of spans to return.
     * @return Batch of pending span snapshots.
     */
    std::vector<SpanSnapshot> pendingSpansSince(std::uint64_t sequence, std::size_t maxCount) const;

private:
    std::size_t capacityBytes_;
    mutable std::mutex mutex_;
    std::deque<SpanSnapshot> spans_;
    std::size_t sizeBytes_ {};
    std::uint64_t nextSequence_ {1};
};

} // namespace jami::telemetry::detail