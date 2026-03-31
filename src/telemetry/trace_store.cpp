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
#include "trace_store.h"

#include "logger.h"

namespace jami::telemetry::detail {

/**
 * @brief TraceStore Creates the bounded span buffer used by the runtime.
 * @param capacityBytes Maximum serialized size retained in memory.
 * @return void
 */
TraceStore::TraceStore(std::size_t capacityBytes)
    : capacityBytes_(capacityBytes)
{}

/**
 * @brief clear Drops all buffered spans and resets store counters.
 * @return void
 */
void
TraceStore::clear()
{
    std::lock_guard lk {mutex_};
    spans_.clear();
    sizeBytes_ = 0;
    nextSequence_ = 1;
}

/**
 * @brief appendCompletedSpan Snapshots a finished span into the ring buffer.
 * @param spanData Completed SDK span data.
 * @return true when the span was buffered successfully.
 */
bool
TraceStore::appendCompletedSpan(const trace_sdk::SpanData& spanData)
{
    auto snapshot = snapshotSpan(spanData, 0);

    std::lock_guard lk {mutex_};
    snapshot.sequence = nextSequence_++;
    snapshot.serializedSize = measureSpanSize(snapshot);

    if (snapshot.serializedSize > capacityBytes_) {
        JAMI_WARNING("[otel] Dropping oversize span {} ({} bytes)", snapshot.name, snapshot.serializedSize);
        return false;
    }

    sizeBytes_ += snapshot.serializedSize;
    spans_.emplace_back(std::move(snapshot));
    while (!spans_.empty() && sizeBytes_ > capacityBytes_) {
        sizeBytes_ -= spans_.front().serializedSize;
        spans_.pop_front();
    }
    return true;
}

/**
 * @brief copySpans Copies the current ring buffer contents for export.
 * @return Snapshot copy of all buffered spans.
 */
std::vector<SpanSnapshot>
TraceStore::copySpans() const
{
    std::lock_guard lk {mutex_};
    return std::vector<SpanSnapshot>(spans_.begin(), spans_.end());
}

/**
 * @brief pendingSpansSince Selects spans newer than the last uploaded sequence.
 * @param sequence Last uploaded sequence number.
 * @param maxCount Maximum number of spans to return.
 * @return Batch of pending span snapshots.
 */
std::vector<SpanSnapshot>
TraceStore::pendingSpansSince(std::uint64_t sequence, std::size_t maxCount) const
{
    std::vector<SpanSnapshot> pending;
    pending.reserve(maxCount);

    std::lock_guard lk {mutex_};
    for (const auto& span : spans_) {
        if (span.sequence <= sequence)
            continue;

        pending.push_back(span);
        if (pending.size() >= maxCount)
            break;
    }
    return pending;
}

} // namespace jami::telemetry::detail