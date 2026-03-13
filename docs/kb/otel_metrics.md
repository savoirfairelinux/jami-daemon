# OpenTelemetry Metrics — C++ API Guide

| Field        | Value       |
|--------------|-------------|
| Status       | draft       |
| Last Updated | 2026-03-13  |

---

## 1. Meter Acquisition Pattern

```cpp
#include "opentelemetry/metrics/provider.h"

namespace metric_api = opentelemetry::metrics;

// Acquire once; safe to cache as a static or class member.
auto meter = metric_api::Provider::GetMeterProvider()
                 ->GetMeter("jami.sip",    // instrumentation scope name
                            "1.0.0");      // scope version
```

Meter objects are lightweight handles. The SDK guarantees the same `Meter` is returned for the same (name, version) pair within one provider.

---

## 2. Instrument Types

### 2.1 Counter (monotonically increasing)

Use for: total events, total bytes sent, total calls started.

```cpp
// Create once (e.g., in constructor / static initializer)
auto calls_started = meter->CreateUInt64Counter(
    "jami.calls.started",          // metric name
    "Total calls initiated",       // description
    "{calls}");                    // unit

// Record
std::map<std::string, std::string> attrs = {
    {"jami.account.type", "RING"},
    {"call.direction",    "outgoing"},
};
auto kv = opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs};
calls_started->Add(1, kv);
```

### 2.2 UpDownCounter (can go up or down)

Use for: current active calls, current connections, queue depth.

```cpp
auto active_calls = meter->CreateInt64UpDownCounter(
    "jami.calls.active",
    "Currently active calls",
    "{calls}");

active_calls->Add(+1, kv);   // call started
active_calls->Add(-1, kv);   // call ended
```

### 2.3 Histogram (value distribution)

Use for: call setup latency, packet round-trip time, message size.

```cpp
auto call_setup_duration = meter->CreateDoubleHistogram(
    "jami.call.setup.duration",
    "Time from INVITE to ESTABLISHED",
    "ms");

// Record a measurement
double duration_ms = /* measured */ 238.5;
call_setup_duration->Record(duration_ms, kv);
```

> **Histogram bucket configuration**: The default buckets are suitable for request latencies. For jami-daemon, consider custom boundaries if measuring DHT lookup times (often 5–500 ms) vs. ICE negotiation times (500 ms–10 s). Configure via Views (see §5).

### 2.4 ObservableGauge (pull-based, current value snapshot)

Use for: memory usage, number of registered accounts, DHT routing table size.

```cpp
auto routing_table_size = meter->CreateInt64ObservableGauge(
    "jami.dht.routing_table.size",
    "Number of nodes in DHT routing table",
    "{nodes}");

// Register callback — called during each metrics export cycle
routing_table_size->AddCallback(
    [](opentelemetry::metrics::ObserverResult result, void* state) {
        auto* dht = static_cast<DhtRunner*>(state);
        result.Observe(static_cast<int64_t>(dht->getRoutingTableSize()));
        // Attributes can also be added:
        // result.Observe(n, {{"jami.dht.family", "IPv4"}});
    },
    static_cast<void*>(dht_runner_ptr));
```

> **Lifetime**: Keep the `ObservableGauge` object alive for as long as you want callbacks to fire. Destroying it unregisters the callback.

### 2.5 ObservableCounter (pull-based, monotonically increasing)

Use for: cumulative bytes sent since startup, reported via callback.

```cpp
auto bytes_sent = meter->CreateInt64ObservableCounter(
    "jami.media.bytes.sent",
    "Cumulative RTP bytes sent",
    "By");

bytes_sent->AddCallback(
    [](opentelemetry::metrics::ObserverResult result, void* state) {
        auto* media = static_cast<MediaEngine*>(state);
        result.Observe(media->getTotalBytesSent());
    },
    static_cast<void*>(media_engine_ptr));
```

---

## 3. Attribute Sets for Measurements

Always use **low-cardinality** attribute values (see §7 Cardinality).

```cpp
// Recommended helper to build attribute maps:
using Attrs = std::map<std::string, std::string>;

Attrs call_attrs = {
    {"jami.account.type", "RING"},        // "RING" | "SIP" | "IAX"
    {"call.direction",    "outgoing"},    // "outgoing" | "incoming"
    {"call.media.type",   "audio"},       // "audio" | "video" | "av"
};
auto kv = opentelemetry::common::KeyValueIterableView<Attrs>{call_attrs};

counter->Add(1, kv);
histogram->Record(latency_ms, kv);
```

---

## 4. Export Interval Configuration

Configured on `PeriodicExportingMetricReader`:

```cpp
metric_sdk::PeriodicExportingMetricReaderOptions opts;
opts.export_interval_millis = std::chrono::milliseconds(15000);  // default: 60s
opts.export_timeout_millis  = std::chrono::milliseconds(5000);   // default: 30s

auto reader = metric_sdk::PeriodicExportingMetricReaderFactory::Create(
                  std::move(exporter), opts);
```

**Recommended for jami-daemon**: 15–30 second intervals. Sub-second intervals create significant overhead on constrained devices.

---

## 5. Views Configuration

Views let you customize aggregation, rename metrics, or filter attributes without changing instrumentation code.

### 5.1 Map Counter to Sum Aggregation (explicit)

```cpp
#include "opentelemetry/sdk/metrics/view/instrument_selector.h"
#include "opentelemetry/sdk/metrics/view/meter_selector.h"
#include "opentelemetry/sdk/metrics/view/view.h"

auto instrument_sel = std::make_unique<metric_sdk::InstrumentSelector>(
    metric_sdk::InstrumentType::kCounter,
    "jami.calls.started");

auto meter_sel = std::make_unique<metric_sdk::MeterSelector>(
    "jami.sip", "1.0.0", "");

auto sum_view = std::make_unique<metric_sdk::View>(
    "jami.calls.started",
    "Total calls initiated",
    metric_sdk::AggregationType::kSum);

// Register with the MeterProvider (sdk pointer):
sdk_meter_provider->AddView(std::move(instrument_sel),
                            std::move(meter_sel),
                            std::move(sum_view));
```

### 5.2 Custom Histogram Boundaries

```cpp
#include "opentelemetry/sdk/metrics/aggregation/histogram_aggregation.h"

auto hist_cfg = std::make_shared<metric_sdk::HistogramAggregationConfig>();
hist_cfg->boundaries_ = {0, 5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000};

auto hist_view = std::make_unique<metric_sdk::View>(
    "jami.call.setup.duration",
    "Call setup time",
    metric_sdk::AggregationType::kHistogram,
    std::move(hist_cfg));
```

---

## 6. Delta vs Cumulative Temporality

| Temporality | Meaning | OTLP default for |
|-------------|---------|-----------------|
| **Cumulative** | Value since process start | Counters, ObservableCounters |
| **Delta** | Value since last export | UpDownCounters, Gauges, Histograms |

The OTLP exporter defaults to **Cumulative** for counters and **Delta** for histograms. To change:

```cpp
otlp::OtlpGrpcMetricExporterOptions opts;
opts.aggregation_temporality = metric_sdk::AggregationTemporality::kDelta;
```

**Prometheus preference**: Prometheus prefers **Cumulative**. When using the Prometheus exporter this is handled automatically.

---

## 7. Cardinality Limits and Warnings

The SDK has a **default cardinality limit of 2000 unique attribute-set combinations per instrument**. Exceeding this causes measurements to be dropped (with a warning log).

### What NOT to use as metric labels

| Bad label | Why it's bad | Safer alternative |
|-----------|-------------|-------------------|
| `jami.call.id` (unique per call) | Unbounded cardinality | Drop from metrics; use only in traces |
| `peer.uri` (per-peer) | Potentially unbounded | Drop; hash to a cohort bucket |
| `jami.account.id` (per user) | Unbounded in multi-user setups | Drop or use account.type |
| Hostname/IP of remote peer | Unbounded | `net.transport`; drop IP from metrics |
| Timestamp variants | Always unique | Never use as attribute |

### Good low-cardinality label examples

```
jami.account.type   = "RING" | "SIP"
call.direction      = "incoming" | "outgoing"
call.media.type     = "audio" | "video" | "av" | "none"
error.type          = "timeout" | "rejected" | "busy" | "unreachable"
net.transport       = "ip_tcp" | "ip_udp" | "ip_tls"
```

---

## Source References

- [OTel C++ Instrumentation (Metrics)](https://opentelemetry.io/docs/languages/cpp/instrumentation/#metrics)
- [Metrics API namespace docs](https://opentelemetry-cpp.readthedocs.io/en/latest/otel_docs/namespace_opentelemetry__metrics.html)
- [Metrics SDK namespace docs](https://opentelemetry-cpp.readthedocs.io/en/latest/otel_docs/namespace_opentelemetry__sdk__metrics.html)
- [examples/metrics_simple](https://github.com/open-telemetry/opentelemetry-cpp/tree/main/examples/metrics_simple)
- [OTel C++ Exporters (Prometheus)](https://opentelemetry.io/docs/languages/cpp/exporters/#prometheus)

---

## Open Questions

1. **ObservableGauge callback thread**: Which thread does the `PeriodicExportingMetricReader` call Observable callbacks on? Is it safe to access `DhtRunner` state from that thread?
2. **Media stats instruments**: Should per-call RTP statistics (jitter, packet loss) be reported as metrics with `{call.session.id}` labels (bounded per-session counters, reset each call) or as span attributes? Per-call metrics with bounded session IDs (reset on export) may be acceptable if the max concurrent calls is low.
3. **Prometheus vs OTLP**: If the deployment team prefers Prometheus scraping, should the daemon expose a `/metrics` endpoint (Prometheus exporter) instead of pushing to a collector?
4. **Custom aggregation for DHT latency**: DHT lookup times can range from 10 ms to 60 seconds. Standard HTTP latency buckets are inappropriate. What boundaries should be specified in the View?
5. **UpDownCounter for active calls**: When the daemon restarts, the UpDownCounter resets to 0. For Prometheus, this means a drop to 0 at restart. Is that acceptable?
