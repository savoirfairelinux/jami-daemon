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
#include "yrs_document.h"

#include <atomic>
#include <mutex>

extern "C" {
#include <libyrs.h>
}

namespace jami {

struct YrsDocument::Impl
{
    YDoc* doc {nullptr};
    std::mutex mutex;
    // Raised by the update observer whenever the document actually moves. It is
    // written from inside ytransaction_commit, with mutex already held by the
    // caller, so it must be lock-free and must never read the document back.
    std::atomic_bool changed {false};
    YSubscription* subscription {nullptr};
};

YrsDocument::YrsDocument(uint64_t clientId)
    : pimpl_(std::make_unique<Impl>())
{
    YOptions options = yoptions();
    options.id = clientId;
    // UTF-16 offsets, matching what the Qt, Android and iOS text editors index
    // with. The daemon never uses an offset itself, but the flag is part of the
    // document's identity and every replica must agree on it.
    options.flags = Y_OFFSET_UTF16;
    pimpl_->doc = ydoc_new_with_options(options);
    // yoptions() hands over ownership of the strings it allocates (guid, and
    // collection_id when set), and ydoc_new_with_options() copies them rather
    // than adopting them. Without this, every document ever opened leaks its
    // guid for the lifetime of the daemon.
    if (options.guid)
        ystring_destroy(const_cast<char*>(options.guid));
    if (options.collection_id)
        ystring_destroy(const_cast<char*>(options.collection_id));
    // yrs only fires this when an update brings something the replica did not
    // already hold, which makes it the one honest answer to "did that change
    // anything". It says nothing about what changed, so the daemon stays as
    // blind to the document's shape as it was without it.
    pimpl_->subscription = ydoc_observe_updates_v1(pimpl_->doc, pimpl_.get(), [](void* state, uint32_t, const char*) {
        static_cast<Impl*>(state)->changed.store(true, std::memory_order_relaxed);
    });
}

YrsDocument::~YrsDocument()
{
    if (pimpl_->subscription)
        yunobserve(pimpl_->subscription);
    if (pimpl_->doc)
        ydoc_destroy(pimpl_->doc);
}

bool
YrsDocument::takeChanged()
{
    return pimpl_->changed.exchange(false, std::memory_order_relaxed);
}

bool
YrsDocument::applyUpdate(const Bytes& update)
{
    if (update.empty())
        return false;
    std::lock_guard<std::mutex> lk(pimpl_->mutex);
    YTransaction* txn = ydoc_write_transaction(pimpl_->doc, 0, nullptr);
    if (!txn)
        return false;
    // Root types are created on demand by the update itself, so the document
    // never has to be told which shared types it holds.
    const auto err = ytransaction_apply(txn, reinterpret_cast<const char*>(update.data()), update.size());
    ytransaction_commit(txn);
    return err == 0;
}

YrsDocument::Bytes
YrsDocument::encodeStateAsUpdate() const
{
    std::lock_guard<std::mutex> lk(pimpl_->mutex);
    YTransaction* txn = ydoc_read_transaction(pimpl_->doc);
    if (!txn)
        return {};
    uint32_t len = 0;
    // A null state vector requests the full document state.
    char* data = ytransaction_state_diff_v1(txn, nullptr, 0, &len);
    Bytes update;
    if (data) {
        update.assign(reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + len);
        ybinary_destroy(data, len);
    }
    ytransaction_commit(txn);
    return update;
}

YrsDocument::Bytes
YrsDocument::encodeStateVector() const
{
    std::lock_guard<std::mutex> lk(pimpl_->mutex);
    YTransaction* txn = ydoc_read_transaction(pimpl_->doc);
    if (!txn)
        return {};
    uint32_t len = 0;
    char* data = ytransaction_state_vector_v1(txn, &len);
    Bytes sv;
    if (data) {
        sv.assign(reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + len);
        ybinary_destroy(data, len);
    }
    ytransaction_commit(txn);
    return sv;
}

YrsDocument::Bytes
YrsDocument::encodeDiff(const Bytes& stateVector) const
{
    if (stateVector.empty())
        return encodeStateAsUpdate();
    std::lock_guard<std::mutex> lk(pimpl_->mutex);
    YTransaction* txn = ydoc_read_transaction(pimpl_->doc);
    if (!txn)
        return {};
    uint32_t len = 0;
    char* data = ytransaction_state_diff_v1(txn,
                                            reinterpret_cast<const char*>(stateVector.data()),
                                            stateVector.size(),
                                            &len);
    Bytes update;
    if (data) {
        update.assign(reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + len);
        ybinary_destroy(data, len);
    }
    ytransaction_commit(txn);
    return update;
}

} // namespace jami
