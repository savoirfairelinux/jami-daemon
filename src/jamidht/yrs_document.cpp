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

#include "json_utils.h"

#include <algorithm>
#include <mutex>

// libyrs.h is a plain C header with no extern "C" guard of its own.
extern "C" {
#include <libyrs.h>
}

namespace jami {

namespace {

// Number of UTF-16 code units in a UTF-8 string (a code point above U+FFFF maps
// to a surrogate pair, i.e. 2 units). Matches Y_OFFSET_UTF16 indexing.
uint32_t
utf16Len(const std::string& utf8)
{
    uint32_t units = 0;
    for (size_t i = 0; i < utf8.size();) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        size_t adv = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3 : (c >> 3) == 0x1e ? 4 : 1;
        units += (adv == 4) ? 2 : 1;
        i += adv;
    }
    return units;
}

// Compact (no-indent) JSON serialization, used for the per-op attribute objects
// and the whole-document delta.
std::string
toCompactJson(const Json::Value& v)
{
    Json::StreamWriterBuilder b;
    b["indentation"] = "";
    return Json::writeString(b, v);
}

Json::Value
yOutputToJson(const YOutput& o)
{
    switch (o.tag) {
    case Y_JSON_BOOL: {
        const uint8_t* b = youtput_read_bool(&o);
        return Json::Value(b && *b != 0);
    }
    case Y_JSON_INT: {
        const int64_t* v = youtput_read_long(&o);
        return Json::Value(static_cast<Json::Int64>(v ? *v : 0));
    }
    case Y_JSON_NUM: {
        const double* v = youtput_read_float(&o);
        return Json::Value(v ? *v : 0.0);
    }
    case Y_JSON_STR: {
        char* s = youtput_read_string(&o);
        return Json::Value(s ? s : "");
    }
    default:
        return Json::Value(Json::nullValue);
    }
}

// Serialize a list of formatting attributes (from a delta op) to a compact JSON
// object string. Returns an empty string when there are none.
std::string
deltaAttrsToJson(const YDeltaAttr* attrs, uint32_t len)
{
    if (!attrs || len == 0)
        return {};
    Json::Value obj(Json::objectValue);
    for (uint32_t i = 0; i < len; ++i)
        obj[attrs[i].key] = yOutputToJson(attrs[i].value);
    return toCompactJson(obj);
}

} // namespace

struct YrsDocument::Impl
{
    YDoc* doc {nullptr};
    Branch* text {nullptr};
    Branch* nameBranch {nullptr};
    YSubscription* updateSub {nullptr};
    YSubscription* textSub {nullptr};
    YSubscription* nameSub {nullptr};

    UpdateCallback updateCb;
    ChangeCallback changeCb;
    NameCallback nameCb;
    RichChangeCallback richChangeCb;

    std::recursive_mutex mutex;
    // True while applyUpdate() is running, so the observe callbacks can tell a
    // remote mutation from a local insert()/remove().
    bool applyingRemote {false};
    // True while seeding a document from persisted commits: the observe callbacks
    // suppress all client notifications, since the caller reads the converged state
    // directly once loading completes.
    bool suppressEmit {false};
    // Set by the name observer (which runs while a transaction is still active and
    // therefore cannot open its own). The full name is read and dispatched after
    // the mutating transaction has closed, via notifyNamePending().
    bool namePending {false};
    bool namePendingLocal {false};

    static void onUpdate(void* state, uint32_t len, const char* data)
    {
        auto* self = static_cast<Impl*>(state);
        if (self->suppressEmit)
            return;
        if (self->updateCb && len > 0) {
            Bytes update(reinterpret_cast<const uint8_t*>(data),
                         reinterpret_cast<const uint8_t*>(data) + len);
            self->updateCb(update, !self->applyingRemote);
        }
    }

    static void onNameChange(void* state, const YTextEvent* event)
    {
        (void) event;
        auto* self = static_cast<Impl*>(state);
        if (self->suppressEmit)
            return;
        // A transaction is still alive here; defer the read until it closes.
        self->namePending = true;
        self->namePendingLocal = !self->applyingRemote;
    }

    // Called by mutators after their transaction has committed (closed), so it can
    // safely open a read transaction to fetch the converged name.
    void notifyNamePending()
    {
        if (!namePending)
            return;
        namePending = false;
        if (!nameCb)
            return;
        YTransaction* txn = ydoc_read_transaction(doc);
        char* str = ytext_string(nameBranch, txn);
        std::string name = str ? str : "";
        if (str)
            ystring_destroy(str);
        ytransaction_commit(txn);
        nameCb(name, namePendingLocal);
    }

    static void onChange(void* state, const YTextEvent* event)
    {
        auto* self = static_cast<Impl*>(state);
        if (self->suppressEmit)
            return;
        if (!self->changeCb && !self->richChangeCb)
            return;
        uint32_t n = 0;
        YDeltaOut* delta = ytext_event_delta(event, &n);
        std::vector<TextChange> changes;
        std::vector<RichOp> richOps;
        uint32_t pos = 0;
        for (uint32_t i = 0; i < n; ++i) {
            std::string attrs = deltaAttrsToJson(delta[i].attributes, delta[i].attributes_len);
            switch (delta[i].tag) {
            case Y_EVENT_CHANGE_RETAIN:
                pos += delta[i].len;
                richOps.push_back({RichOp::Kind::Retain, delta[i].len, std::string {}, attrs});
                break;
            case Y_EVENT_CHANGE_DELETE:
                changes.push_back({pos, delta[i].len, std::string {}});
                richOps.push_back({RichOp::Kind::Delete, delta[i].len, std::string {}, std::string {}});
                break;
            case Y_EVENT_CHANGE_ADD: {
                char* str = youtput_read_string(delta[i].insert);
                std::string text = str ? std::string {str} : std::string {};
                changes.push_back({pos, 0, text});
                richOps.push_back({RichOp::Kind::Insert, delta[i].len, text, attrs});
                // In UTF-16 offset mode, len is the inserted code unit count.
                pos += delta[i].len;
                break;
            }
            default:
                break;
            }
        }
        ytext_delta_destroy(delta, n);
        if (self->changeCb && !changes.empty())
            self->changeCb(changes, !self->applyingRemote);
        if (self->richChangeCb && !richOps.empty())
            self->richChangeCb(richOps, !self->applyingRemote);
    }
};

YrsDocument::YrsDocument(uint64_t clientId)
    : pimpl_(std::make_unique<Impl>())
{
    YOptions options = yoptions();
    options.id = clientId;
    options.flags = Y_OFFSET_UTF16; // UTF-16 offsets to match the clients' editors
    pimpl_->doc = ydoc_new_with_options(options);
    pimpl_->text = ytext(pimpl_->doc, "content");
    pimpl_->nameBranch = ytext(pimpl_->doc, "name");
    pimpl_->updateSub = ydoc_observe_updates_v1(pimpl_->doc, pimpl_.get(), &Impl::onUpdate);
    pimpl_->textSub = ytext_observe(pimpl_->text, pimpl_.get(), &Impl::onChange);
    pimpl_->nameSub = ytext_observe(pimpl_->nameBranch, pimpl_.get(), &Impl::onNameChange);
}

YrsDocument::~YrsDocument()
{
    if (pimpl_->updateSub)
        yunobserve(pimpl_->updateSub);
    if (pimpl_->textSub)
        yunobserve(pimpl_->textSub);
    if (pimpl_->nameSub)
        yunobserve(pimpl_->nameSub);
    if (pimpl_->doc)
        ydoc_destroy(pimpl_->doc);
}

void
YrsDocument::setUpdateCallback(UpdateCallback cb)
{
    std::lock_guard<std::recursive_mutex> lk(pimpl_->mutex);
    pimpl_->updateCb = std::move(cb);
}

void
YrsDocument::setChangeCallback(ChangeCallback cb)
{
    std::lock_guard<std::recursive_mutex> lk(pimpl_->mutex);
    pimpl_->changeCb = std::move(cb);
}

void
YrsDocument::setNameCallback(NameCallback cb)
{
    std::lock_guard<std::recursive_mutex> lk(pimpl_->mutex);
    pimpl_->nameCb = std::move(cb);
}

void
YrsDocument::setRichChangeCallback(RichChangeCallback cb)
{
    std::lock_guard<std::recursive_mutex> lk(pimpl_->mutex);
    pimpl_->richChangeCb = std::move(cb);
}

void
YrsDocument::applyDelta(const std::vector<RichOp>& ops)
{
    if (ops.empty())
        return;
    std::lock_guard<std::recursive_mutex> lk(pimpl_->mutex);
    YTransaction* txn = ydoc_write_transaction(pimpl_->doc, 0, nullptr);
    uint32_t index = 0;
    for (const auto& op : ops) {
        // Clamp every offset/length to the live document length. yrs panics (and
        // aborts the whole process) on an out-of-range index, so this is a hard
        // safety net against any client/CRDT desync.
        uint32_t len = ytext_len(pimpl_->text, txn);
        if (index > len)
            index = len;
        switch (op.kind) {
        case RichOp::Kind::Retain: {
            uint32_t n = std::min(op.len, len - index);
            if (!op.attrs.empty() && n > 0) {
                YInput attr = yinput_json(op.attrs.c_str());
                ytext_format(pimpl_->text, txn, index, n, &attr);
            }
            index += op.len;
            break;
        }
        case RichOp::Kind::Insert: {
            if (!op.text.empty()) {
                if (op.attrs.empty()) {
                    ytext_insert(pimpl_->text, txn, index, op.text.c_str(), nullptr);
                } else {
                    YInput attr = yinput_json(op.attrs.c_str());
                    ytext_insert(pimpl_->text, txn, index, op.text.c_str(), &attr);
                }
                index += utf16Len(op.text);
            }
            break;
        }
        case RichOp::Kind::Delete: {
            uint32_t n = std::min(op.len, len - index);
            if (n > 0)
                ytext_remove_range(pimpl_->text, txn, index, n);
            break;
        }
        }
    }
    ytransaction_commit(txn);
}

std::string
YrsDocument::contentDelta() const
{
    std::lock_guard<std::recursive_mutex> lk(pimpl_->mutex);
    YTransaction* txn = ydoc_read_transaction(pimpl_->doc);
    uint32_t n = 0;
    YChunk* chunks = ytext_chunks(pimpl_->text, txn, &n);
    Json::Value arr(Json::arrayValue);
    for (uint32_t i = 0; i < n; ++i) {
        char* s = youtput_read_string(&chunks[i].data);
        Json::Value op(Json::objectValue);
        op["insert"] = s ? s : "";
        if (chunks[i].fmt_len > 0) {
            Json::Value attrs(Json::objectValue);
            for (uint32_t j = 0; j < chunks[i].fmt_len; ++j) {
                const auto& entry = chunks[i].fmt[j];
                attrs[entry.key] = entry.value ? yOutputToJson(*entry.value)
                                               : Json::Value(Json::nullValue);
            }
            op["attributes"] = attrs;
        }
        arr.append(op);
    }
    if (chunks)
        ychunks_destroy(chunks, n);
    ytransaction_commit(txn);
    return toCompactJson(arr);
}

void
YrsDocument::setName(const std::string& utf8Name)
{
    std::lock_guard<std::recursive_mutex> lk(pimpl_->mutex);
    YTransaction* txn = ydoc_write_transaction(pimpl_->doc, 0, nullptr);
    auto len = ytext_len(pimpl_->nameBranch, txn);
    if (len > 0)
        ytext_remove_range(pimpl_->nameBranch, txn, 0, len);
    if (!utf8Name.empty())
        ytext_insert(pimpl_->nameBranch, txn, 0, utf8Name.c_str(), nullptr);
    ytransaction_commit(txn);
    pimpl_->notifyNamePending();
}

std::string
YrsDocument::name() const
{
    std::lock_guard<std::recursive_mutex> lk(pimpl_->mutex);
    YTransaction* txn = ydoc_read_transaction(pimpl_->doc);
    char* str = ytext_string(pimpl_->nameBranch, txn);
    std::string result = str ? str : "";
    if (str)
        ystring_destroy(str);
    ytransaction_commit(txn);
    return result;
}

void
YrsDocument::insert(uint32_t index, const std::string& utf8Text)
{
    std::lock_guard<std::recursive_mutex> lk(pimpl_->mutex);
    YTransaction* txn = ydoc_write_transaction(pimpl_->doc, 0, nullptr);
    ytext_insert(pimpl_->text, txn, index, utf8Text.c_str(), nullptr);
    ytransaction_commit(txn);
}

void
YrsDocument::remove(uint32_t index, uint32_t length)
{
    std::lock_guard<std::recursive_mutex> lk(pimpl_->mutex);
    YTransaction* txn = ydoc_write_transaction(pimpl_->doc, 0, nullptr);
    ytext_remove_range(pimpl_->text, txn, index, length);
    ytransaction_commit(txn);
}

void
YrsDocument::applyUpdate(const Bytes& update, bool silent)
{
    if (update.empty())
        return;
    std::lock_guard<std::recursive_mutex> lk(pimpl_->mutex);
    pimpl_->applyingRemote = true;
    pimpl_->suppressEmit = silent;
    YTransaction* txn = ydoc_write_transaction(pimpl_->doc, 0, nullptr);
    ytransaction_apply(txn, reinterpret_cast<const char*>(update.data()), update.size());
    ytransaction_commit(txn);
    pimpl_->applyingRemote = false;
    pimpl_->suppressEmit = false;
    if (!silent)
        pimpl_->notifyNamePending();
}

std::string
YrsDocument::text() const
{
    std::lock_guard<std::recursive_mutex> lk(pimpl_->mutex);
    YTransaction* txn = ydoc_read_transaction(pimpl_->doc);
    char* str = ytext_string(pimpl_->text, txn);
    std::string result = str ? str : "";
    if (str)
        ystring_destroy(str);
    ytransaction_commit(txn);
    return result;
}

YrsDocument::Bytes
YrsDocument::encodeStateAsUpdate() const
{
    std::lock_guard<std::recursive_mutex> lk(pimpl_->mutex);
    YTransaction* txn = ydoc_read_transaction(pimpl_->doc);
    uint32_t len = 0;
    // A null state vector requests the full document state.
    char* data = ytransaction_state_diff_v1(txn, nullptr, 0, &len);
    Bytes update;
    if (data) {
        update.assign(reinterpret_cast<const uint8_t*>(data),
                      reinterpret_cast<const uint8_t*>(data) + len);
        ybinary_destroy(data, len);
    }
    ytransaction_commit(txn);
    return update;
}

} // namespace jami
