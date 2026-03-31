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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "telemetry_runtime.h"

#include "logger.h"
#include "ring_buffer_span_exporter.h"
#include "trace_export.h"

#include <opentelemetry/common/key_value_iterable.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/trace/noop.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/span_startoptions.h>
#include <opentelemetry/trace/tracer.h>

#include <vector>

namespace jami::telemetry {

namespace trace_api = opentelemetry::trace;
namespace resource = opentelemetry::sdk::resource;
namespace common = opentelemetry::common;

namespace {

opentelemetry::nostd::string_view
toOtelStringView(std::string_view value)
{
    return opentelemetry::nostd::string_view(value.data(), value.size());
}

class PublicAttributeIterable final : public common::KeyValueIterable
{
public:
    explicit PublicAttributeIterable(const Attributes& attributes)
        : attributes_(attributes)
    {}

    /**
     * @brief ForEachKeyValue Exposes public span attributes through the SDK iterable API.
     * @param callback Visitor called for each attribute pair.
     * @return false when the visitor stops iteration early.
     */
    bool ForEachKeyValue(opentelemetry::nostd::function_ref<bool(opentelemetry::nostd::string_view,
                                                                 common::AttributeValue)> callback) const noexcept override
    {
        for (const auto& attribute : attributes_) {
            const bool keepGoing = std::visit(detail::Overloaded {
                [&](bool item) { return callback(attribute.key, item); },
                [&](std::int64_t item) { return callback(attribute.key, item); },
                [&](double item) { return callback(attribute.key, item); },
                [&](const std::string& item) { return callback(attribute.key, opentelemetry::nostd::string_view(item)); },
            }, attribute.value);
            if (!keepGoing)
                return false;
        }
        return true;
    }

    std::size_t size() const noexcept override { return attributes_.size(); }

private:
    const Attributes& attributes_;
};

/**
 * @brief applyAttribute Applies one public attribute value to a live SDK span.
 * @param span Live SDK span.
 * @param key Attribute name.
 * @param value Attribute value.
 * @return void
 */
void
applyAttribute(trace_api::Span& span, std::string_view key, const AttributeValue& value)
{
    std::visit(detail::Overloaded {
        [&](bool item) { span.SetAttribute(toOtelStringView(key), item); },
        [&](std::int64_t item) { span.SetAttribute(toOtelStringView(key), item); },
        [&](double item) { span.SetAttribute(toOtelStringView(key), item); },
        [&](const std::string& item) { span.SetAttribute(toOtelStringView(key), opentelemetry::nostd::string_view(item)); },
    }, value);
}

/**
 * @brief applyAttributes Applies all public attributes to a live SDK span.
 * @param span Live SDK span.
 * @param attributes Attributes to write onto the span.
 * @return void
 */
void
applyAttributes(trace_api::Span& span, const Attributes& attributes)
{
    for (const auto& attribute : attributes)
        applyAttribute(span, attribute.key, attribute.value);
}

/**
 * @brief toOtelStatus Converts the public span status enum to the SDK enum.
 * @param status Public status value.
 * @return Matching OpenTelemetry status code.
 */
trace_api::StatusCode
toOtelStatus(SpanStatus status)
{
    switch (status) {
    case SpanStatus::ok:
        return trace_api::StatusCode::kOk;
    case SpanStatus::error:
        return trace_api::StatusCode::kError;
    case SpanStatus::unset:
    default:
        return trace_api::StatusCode::kUnset;
    }
}

} // namespace

struct SpanHandle::Impl
{
    explicit Impl(opentelemetry::nostd::shared_ptr<trace_api::Span> span)
        : span_(std::move(span))
    {}

    bool valid() const noexcept
    {
        std::lock_guard lk {mutex_};
        return span_ != nullptr;
    }

    trace_api::SpanContext context() const noexcept
    {
        std::lock_guard lk {mutex_};
        return span_ ? span_->GetContext() : trace_api::SpanContext::GetInvalid();
    }

    void setAttribute(std::string_view key, const AttributeValue& value)
    {
        std::lock_guard lk {mutex_};
        if (!span_)
            return;
        applyAttribute(*span_, key, value);
    }

    void addEvent(std::string_view name, const Attributes& attributes)
    {
        std::lock_guard lk {mutex_};
        if (!span_)
            return;

        if (attributes.empty()) {
            span_->AddEvent(toOtelStringView(name));
            return;
        }

        PublicAttributeIterable iterable(attributes);
        span_->AddEvent(toOtelStringView(name), iterable);
    }

    /**
     * @brief end Applies closing metadata and finalizes the owned span once.
     * @param options Final status and attributes to attach before ending.
     * @return void
     */
    void end(const SpanEndOptions& options)
    {
        std::lock_guard lk {mutex_};
        if (!span_)
            return;

        applyAttributes(*span_, options.attributes);
        if (options.status != SpanStatus::unset || !options.statusDescription.empty())
            span_->SetStatus(toOtelStatus(options.status), toOtelStringView(options.statusDescription));
        span_->End();
        span_ = nullptr;
    }

private:
    mutable std::mutex mutex_;
    opentelemetry::nostd::shared_ptr<trace_api::Span> span_;
};

SpanHandle::SpanHandle() noexcept = default;

SpanHandle::SpanHandle(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{}

SpanHandle::SpanHandle(SpanHandle&&) noexcept = default;

SpanHandle&
SpanHandle::operator=(SpanHandle&&) noexcept = default;

SpanHandle::~SpanHandle()
{
    if (impl_)
        impl_->end({});
}

bool
SpanHandle::valid() const noexcept
{
    return impl_ && impl_->valid();
}

void
SpanHandle::setAttribute(std::string_view key, bool value)
{
    setAttribute(key, AttributeValue(value));
}

void
SpanHandle::setAttribute(std::string_view key, std::int64_t value)
{
    setAttribute(key, AttributeValue(value));
}

void
SpanHandle::setAttribute(std::string_view key, double value)
{
    setAttribute(key, AttributeValue(value));
}

void
SpanHandle::setAttribute(std::string_view key, const char* value)
{
    setAttribute(key, AttributeValue(std::string(value ? value : "")));
}

void
SpanHandle::setAttribute(std::string_view key, std::string value)
{
    setAttribute(key, AttributeValue(std::move(value)));
}

void
SpanHandle::setAttribute(std::string_view key, AttributeValue value)
{
    if (impl_)
        impl_->setAttribute(key, value);
}

void
SpanHandle::addEvent(std::string_view name, const Attributes& attributes)
{
    if (impl_)
        impl_->addEvent(name, attributes);
}

void
SpanHandle::end(const SpanEndOptions& options)
{
    if (impl_)
        impl_->end(options);
}

namespace detail {

TelemetryRuntime::TelemetryRuntime()
    : store_()
    , uploader_(store_)
{}

void
TelemetryRuntime::init(const std::string& serviceName,
                       const std::string& version,
                       const std::string& deviceId)
{
    std::lock_guard lk {lifecycleMutex_};
    if (initialized_) {
        JAMI_WARNING("[otel] initTelemetry called more than once, ignoring");
        return;
    }

    try {
        serviceName_ = serviceName;
        serviceVersion_ = version;
        deviceId_ = deviceId;
        store_.clear();

        resource::ResourceAttributes resourceAttrs;
        resourceAttrs["service.name"] = serviceName;
        resourceAttrs["service.version"] = version;
        resourceAttrs["telemetry.sdk.language"] = std::string("cpp");
        resourceAttrs["service.instance.id"] = deviceId;
        resourceAttrs["jami.device_id"] = deviceId;

        std::vector<std::unique_ptr<trace_sdk::SpanProcessor>> processors;
        processors.push_back(trace_sdk::SimpleSpanProcessorFactory::Create(
            std::make_unique<RingBufferSpanExporter>(store_, [this] { notifySpanBuffered(); })));

        auto provider = std::make_unique<trace_sdk::TracerProvider>(
            std::move(processors), resource::Resource::Create(resourceAttrs));
        sdkProvider_ = provider.get();
        trace_api::Provider::SetTracerProvider(
            opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(provider.release()));

        uploader_.start();
        initialized_ = true;

        JAMI_LOG("[otel] Telemetry initialized service={} version={} device_id={}",
                 serviceName,
                 version,
                 deviceId);
    } catch (const std::exception& e) {
        sdkProvider_ = nullptr;
        serviceName_.clear();
        serviceVersion_.clear();
        deviceId_.clear();
        trace_api::Provider::SetTracerProvider(
            opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(new trace_api::NoopTracerProvider));
        JAMI_ERROR("[otel] Failed to initialize telemetry: {}", e.what());
    } catch (...) {
        sdkProvider_ = nullptr;
        serviceName_.clear();
        serviceVersion_.clear();
        deviceId_.clear();
        trace_api::Provider::SetTracerProvider(
            opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(new trace_api::NoopTracerProvider));
        JAMI_ERROR("[otel] Failed to initialize telemetry (unknown error)");
    }
}

void
TelemetryRuntime::shutdown()
{
    trace_sdk::TracerProvider* provider = nullptr;

    {
        std::lock_guard lk {lifecycleMutex_};
        if (!initialized_)
            return;

        initialized_ = false;
        provider = sdkProvider_;
        sdkProvider_ = nullptr;
    }

    uploader_.stop();

    try {
        if (provider) {
            provider->ForceFlush();
            provider->Shutdown();
        }
    } catch (const std::exception& e) {
        JAMI_WARNING("[otel] Exception during telemetry shutdown: {}", e.what());
    } catch (...) {
        JAMI_WARNING("[otel] Unknown exception during telemetry shutdown");
    }

    trace_api::Provider::SetTracerProvider(
        opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(new trace_api::NoopTracerProvider));

    {
        std::lock_guard lk {lifecycleMutex_};
        serviceName_.clear();
        serviceVersion_.clear();
        deviceId_.clear();
    }
    store_.clear();
    JAMI_LOG("[otel] Telemetry shut down");
}

bool
TelemetryRuntime::isInitialized() const noexcept
{
    std::lock_guard lk {lifecycleMutex_};
    return initialized_;
}

SpanHandle
TelemetryRuntime::startSpan(std::string_view name, const SpanStartOptions& options)
{
    std::string version;
    std::string deviceId;
    {
        std::lock_guard lk {lifecycleMutex_};
        if (!initialized_)
            return {};
        version = serviceVersion_;
        deviceId = deviceId_;
    }

    try {
        const std::string scopeName = options.scope.empty() ? std::string("jami.daemon") : options.scope;
        auto tracer = trace_api::Provider::GetTracerProvider()->GetTracer(scopeName, version);
        auto span = tracer->StartSpan(toOtelStringView(name));

        if (!deviceId.empty())
            span->SetAttribute("jami.device_id", opentelemetry::nostd::string_view(deviceId));
        applyAttributes(*span, options.attributes);
        return SpanHandle(std::make_unique<SpanHandle::Impl>(std::move(span)));
    } catch (const std::exception& e) {
        JAMI_WARNING("[otel] Unable to start span {}: {}", std::string(name), e.what());
    } catch (...) {
        JAMI_WARNING("[otel] Unable to start span {}", std::string(name));
    }
    return {};
}

SpanHandle
TelemetryRuntime::startChildSpan(const SpanHandle& parent,
                                 std::string_view name,
                                 const SpanStartOptions& options)
{
    if (!parent.impl_)
        return {};

    const auto parentContext = parent.impl_->context();
    if (!parentContext.IsValid())
        return {};

    std::string version;
    std::string deviceId;
    {
        std::lock_guard lk {lifecycleMutex_};
        if (!initialized_)
            return {};
        version = serviceVersion_;
        deviceId = deviceId_;
    }

    try {
        trace_api::StartSpanOptions spanOptions;
        spanOptions.parent = parentContext;

        const std::string scopeName = options.scope.empty() ? std::string("jami.daemon") : options.scope;
        auto tracer = trace_api::Provider::GetTracerProvider()->GetTracer(scopeName, version);
        auto span = tracer->StartSpan(toOtelStringView(name), spanOptions);

        if (!deviceId.empty())
            span->SetAttribute("jami.device_id", opentelemetry::nostd::string_view(deviceId));
        applyAttributes(*span, options.attributes);
        return SpanHandle(std::make_unique<SpanHandle::Impl>(std::move(span)));
    } catch (const std::exception& e) {
        JAMI_WARNING("[otel] Unable to start child span {}: {}", std::string(name), e.what());
    } catch (...) {
        JAMI_WARNING("[otel] Unable to start child span {}", std::string(name));
    }

    return {};
}

void
TelemetryRuntime::recordTrace(std::string_view name, const Attributes& attributes)
{
    auto span = startSpan(name, {.attributes = attributes});
    span.end();
}

std::string
TelemetryRuntime::exportTraces(const std::string& destinationPath)
{
    std::string serviceName;
    std::string serviceVersion;
    std::string deviceId;

    {
        std::lock_guard lk {lifecycleMutex_};
        if (!initialized_)
            return {};

        serviceName = serviceName_;
        serviceVersion = serviceVersion_;
        deviceId = deviceId_;
    }

    auto spans = store_.copySpans();
    return exportBufferedTraces(serviceName, serviceVersion, deviceId, spans, destinationPath);
}

void
TelemetryRuntime::notifySpanBuffered()
{
    uploader_.notifySpanBuffered();
}

/**
 * @brief runtime Returns the single runtime used by the public telemetry facade.
 * @return Process-wide telemetry runtime instance.
 */
TelemetryRuntime&
runtime()
{
    static TelemetryRuntime instance;
    return instance;
}

} // namespace detail
} // namespace jami::telemetry