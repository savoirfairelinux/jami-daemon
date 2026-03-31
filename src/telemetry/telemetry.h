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

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace jami {
namespace telemetry {

namespace detail {
class TelemetryRuntime;
}

using AttributeValue = std::variant<bool, std::int64_t, double, std::string>;

struct Attribute
{
	std::string key;
	AttributeValue value;

	Attribute(std::string attributeKey, bool attributeValue)
		: key(std::move(attributeKey))
		, value(attributeValue)
	{}

	Attribute(std::string attributeKey, std::int64_t attributeValue)
		: key(std::move(attributeKey))
		, value(attributeValue)
	{}

	Attribute(std::string attributeKey, double attributeValue)
		: key(std::move(attributeKey))
		, value(attributeValue)
	{}

	Attribute(std::string attributeKey, const char* attributeValue)
		: key(std::move(attributeKey))
		, value(std::string(attributeValue ? attributeValue : ""))
	{}

	Attribute(std::string attributeKey, std::string attributeValue)
		: key(std::move(attributeKey))
		, value(std::move(attributeValue))
	{}
};

using Attributes = std::vector<Attribute>;

enum class SpanStatus
{
	unset,
	ok,
	error,
};

struct SpanStartOptions
{
	std::string scope {"jami.daemon"};
	Attributes attributes;
};

struct SpanEndOptions
{
	SpanStatus status {SpanStatus::unset};
	std::string statusDescription;
	Attributes attributes;
};

class SpanHandle
{
public:
	SpanHandle() noexcept;
	SpanHandle(SpanHandle&&) noexcept;
	SpanHandle& operator=(SpanHandle&&) noexcept;
	/**
	 * @brief ~SpanHandle Ends the span if it is still open.
	 * @return void
	 */
	~SpanHandle();

	SpanHandle(const SpanHandle&) = delete;
	SpanHandle& operator=(const SpanHandle&) = delete;

	/**
	 * @brief valid Checks whether this handle still owns a live span.
	 * @return true when the span can still receive updates.
	 */
	bool valid() const noexcept;

	void setAttribute(std::string_view key, bool value);
	void setAttribute(std::string_view key, std::int64_t value);
	void setAttribute(std::string_view key, double value);
	void setAttribute(std::string_view key, const char* value);
	void setAttribute(std::string_view key, std::string value);
	/**
	 * @brief setAttribute Stores or replaces an attribute on the live span.
	 * @param key Attribute name.
	 * @param value Attribute value.
	 * @return void
	 */
	void setAttribute(std::string_view key, AttributeValue value);

	/**
	 * @brief addEvent Appends an event to the live span.
	 * @param name Event name.
	 * @param attributes Event attributes.
	 * @return void
	 */
	void addEvent(std::string_view name, const Attributes& attributes = {});
	/**
	 * @brief end Finalizes the span and applies any closing metadata.
	 * @param options Final status and attributes to attach before closing.
	 * @return void
	 */
	void end(const SpanEndOptions& options = {});

private:
	struct Impl;

	explicit SpanHandle(std::unique_ptr<Impl> impl) noexcept;

	std::unique_ptr<Impl> impl_;

	friend class detail::TelemetryRuntime;
};

/**
 * @brief initTelemetry Initializes the daemon telemetry runtime.
 * @param serviceName Service name reported in telemetry resources.
 * @param version Service version reported in telemetry resources.
 * @param deviceId Local device identifier attached to spans.
 * @return void
 */
void initTelemetry(const std::string& serviceName,
				   const std::string& version,
				   const std::string& deviceId);

/**
 * @brief shutdownTelemetry Stops telemetry export and clears runtime state.
 * @return void
 */
void shutdownTelemetry();

/**
 * @brief isInitialized Reports whether the telemetry runtime is ready.
 * @return true when spans can be created and exported.
 */
bool isInitialized() noexcept;

/**
 * @brief startSpan Starts a new root span on the daemon tracer.
 * @param name Span name.
 * @param options Span scope and initial attributes.
 * @return Move-only handle that controls the live span.
 */
SpanHandle startSpan(std::string_view name,
				  const SpanStartOptions& options = {});

/**
 * @brief startChildSpan Starts a span that uses another span as its parent.
 * @param parent Parent span handle.
 * @param name Child span name.
 * @param options Span scope and initial attributes.
 * @return Move-only handle that controls the live child span.
 */
SpanHandle startChildSpan(const SpanHandle& parent,
					  std::string_view name,
					  const SpanStartOptions& options = {});

/**
 * @brief recordTrace Records a one-shot span with string attributes.
 * @param name Span name.
 * @param attributes String attributes to attach to the span.
 * @return void
 */
void recordTrace(const std::string& name,
				 const std::map<std::string, std::string>& attributes = {});

/**
 * @brief exportTraces Writes the buffered spans to a local JSON file.
 * @param destinationPath Target file path or empty for the default cache path.
 * @return Exported file path, or an empty string on failure.
 */
std::string exportTraces(const std::string& destinationPath = {});

} // namespace telemetry
} // namespace jami
