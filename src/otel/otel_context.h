// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024-2026 Savoir-faire Linux Inc.
#pragma once

#ifdef ENABLE_OTEL
#  include <opentelemetry/context/runtime_context.h>
#  include <opentelemetry/trace/span.h>
#  include <opentelemetry/trace/span_context.h>
#  include <opentelemetry/trace/tracer.h>
#  include "otel_init.h"
#endif

#include <string_view>

namespace jami {
namespace otel {

// ─────────────────────────────────────────────────────────────────────────────
/// RAII span scope guard.
///
/// On construction a child span is started under the current active context.
/// The span is ended and the context token released on destruction.
///
/// Usage:
/// ```cpp
/// {
///     SpanScope span("jami.call", "call.setup");
///     span.setAttribute(attr::CALL_TYPE, "audio");
///     // … do work …
/// } // span ends here
/// ```
///
/// When ENABLE_OTEL is not defined the class compiles to an empty shell with
/// no overhead.
// ─────────────────────────────────────────────────────────────────────────────
class SpanScope
{
public:
    /// Start a child span of the current active context.
#ifdef ENABLE_OTEL
    SpanScope(std::string_view tracer_name,
              std::string_view span_name,
              opentelemetry::trace::SpanKind kind
                  = opentelemetry::trace::SpanKind::kInternal)
        : span_{[&] {
              auto tracer = getTracer(tracer_name);
              opentelemetry::trace::StartSpanOptions opts;
              opts.kind = kind;
              return tracer->StartSpan(std::string(span_name), opts);
          }()},
          scope_{opentelemetry::trace::Tracer::WithActiveSpan(span_)}
    {}

    /// Start a span parented to an explicit SpanContext.
    SpanScope(std::string_view tracer_name,
              std::string_view span_name,
              const opentelemetry::trace::SpanContext& parent_ctx,
              opentelemetry::trace::SpanKind kind
                  = opentelemetry::trace::SpanKind::kInternal)
        : span_{[&] {
              auto tracer = getTracer(tracer_name);
              opentelemetry::trace::StartSpanOptions opts;
              opts.kind   = kind;
              opts.parent = parent_ctx;
              return tracer->StartSpan(std::string(span_name), opts);
          }()},
          scope_{opentelemetry::trace::Tracer::WithActiveSpan(span_)}
    {}
#else
    SpanScope(std::string_view /*tracer_name*/,
              std::string_view /*span_name*/,
              int /*kind*/ = 0)
    {}

    SpanScope(std::string_view /*tracer_name*/,
              std::string_view /*span_name*/,
              const void* /*parent_ctx*/,
              int /*kind*/ = 0)
    {}
#endif

    ~SpanScope()
    {
#ifdef ENABLE_OTEL
        if (span_)
            span_->End();
#endif
    }

    // ── Attribute setters ─────────────────────────────────────────────────────

    void setAttribute(std::string_view key, bool value)
    {
#ifdef ENABLE_OTEL
        if (span_)
            span_->SetAttribute(std::string(key), value);
#else
        (void)key; (void)value;
#endif
    }

    void setAttribute(std::string_view key, int64_t value)
    {
#ifdef ENABLE_OTEL
        if (span_)
            span_->SetAttribute(std::string(key), value);
#else
        (void)key; (void)value;
#endif
    }

    void setAttribute(std::string_view key, double value)
    {
#ifdef ENABLE_OTEL
        if (span_)
            span_->SetAttribute(std::string(key), value);
#else
        (void)key; (void)value;
#endif
    }

    void setAttribute(std::string_view key, std::string_view value)
    {
#ifdef ENABLE_OTEL
        if (span_)
            span_->SetAttribute(std::string(key), std::string(value));
#else
        (void)key; (void)value;
#endif
    }

    // ── Error recording ───────────────────────────────────────────────────────

    void recordError(std::string_view message)
    {
#ifdef ENABLE_OTEL
        if (span_) {
            span_->SetStatus(opentelemetry::trace::StatusCode::kError, std::string(message));
            opentelemetry::common::KeyValueIterableView<std::initializer_list<
                std::pair<opentelemetry::nostd::string_view,
                          opentelemetry::common::AttributeValue>>>
                attrs {{{"exception.message", std::string(message)}}};
            span_->AddEvent("exception", attrs);
        }
#else
        (void)message;
#endif
    }

    // ── Context propagation ───────────────────────────────────────────────────

#ifdef ENABLE_OTEL
    opentelemetry::trace::SpanContext getContext() const
    {
        if (span_)
            return span_->GetContext();
        return opentelemetry::trace::SpanContext::GetInvalid();
    }

    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span() const
    {
        return span_;
    }
#else
    // Return a dummy value; callers are expected to guard with #ifdef ENABLE_OTEL.
    void* getContext() const { return nullptr; }
#endif

    // Non-copyable
    SpanScope(const SpanScope&)            = delete;
    SpanScope& operator=(const SpanScope&) = delete;

    // Movable
    SpanScope(SpanScope&&)            = default;
    SpanScope& operator=(SpanScope&&) = default;

private:
#ifdef ENABLE_OTEL
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
    opentelemetry::trace::Scope                                  scope_;
#endif
};

// ─────────────────────────────────────────────────────────────────────────────
/// Async span helper.
///
/// Used when a span starts in one thread and must be ended in an async
/// callback or a different thread.  Unlike SpanScope, the caller is
/// responsible for calling end().
///
/// Usage:
/// ```cpp
/// auto async = AsyncSpan::start("jami.dht", "dht.lookup");
/// threadPool.post([async]() mutable {
///     // … do async work …
///     async.end(/*success=*/true);
/// });
/// ```
// ─────────────────────────────────────────────────────────────────────────────
struct AsyncSpan
{
#ifdef ENABLE_OTEL
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span;
    opentelemetry::trace::SpanContext                            ctx
        {opentelemetry::trace::SpanContext::GetInvalid()};

    static AsyncSpan start(
        std::string_view tracer_name,
        std::string_view span_name,
        opentelemetry::trace::SpanKind kind = opentelemetry::trace::SpanKind::kInternal)
    {
        AsyncSpan as;
        auto tracer = getTracer(tracer_name);
        opentelemetry::trace::StartSpanOptions opts;
        opts.kind = kind;
        as.span = tracer->StartSpan(std::string(span_name), opts);
        as.ctx  = as.span->GetContext();
        return as;
    }

    void end(bool success = true, std::string_view error_message = "")
    {
        if (!span)
            return;
        if (!success) {
            span->SetStatus(opentelemetry::trace::StatusCode::kError,
                            std::string(error_message));
        }
        span->End();
        span = nullptr;
    }

    bool valid() const { return span != nullptr && span->IsRecording(); }

#else // !ENABLE_OTEL — all no-ops

    static AsyncSpan start(std::string_view /*tracer_name*/,
                           std::string_view /*span_name*/,
                           int /*kind*/ = 0)
    {
        return {};
    }

    void end(bool /*success*/ = true, std::string_view /*error_message*/ = "") {}
    bool valid() const { return false; }

#endif
};

} // namespace otel
} // namespace jami
