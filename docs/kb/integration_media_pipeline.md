# Integration Plan: Media Pipeline

## Status: draft
## Last Updated: 2026-03-13

---

## CRITICAL: Metrics-Only Policy

### Why the per-packet hot path MUST NOT generate spans

The `AudioSender::update()` and `VideoSender::encodeAndSendVideo()` methods are invoked by `ThreadLoop::process()` at a rate of **50 iterations per second for audio** (20 ms Opus frames) and **up to 30 iterations per second for video**. Under these constraints, per-packet OTel span creation is categorically prohibited:

1. **Allocation cost.** Every `Tracer::StartSpan()` call allocates a `SpanData` struct on the heap and registers it in an active-spans map. At 50 audio spans/second × number of concurrent calls, allocator pressure competes with the real-time encoder loop and can introduce encode latency outliers.

2. **Exporter queue contention.** Spans are buffered in the `BatchSpanProcessor` queue. A 50-calls deployment generating 50 audio + 30 video spans/second/call produces **4 000 spans/second** — saturating any reasonable queue size within seconds.

3. **Context propagation overhead.** Even a `Context::GetValue()` lookup per packet adds measurable CPU time to a loop that must complete in < 20 ms (audio) or < 33 ms (video) to avoid buffer underrun.

4. **No diagnostic value at per-packet granularity.** A single dropped RTP packet is invisible at the span level; what matters are aggregate rates (packet-loss %, mean jitter) extracted from RTCP Receiver Reports already computed every 4 seconds by `AudioRtpSession::processRtcpChecker()` and `VideoRtpSession`.

### What IS acceptable on the hot path

OTel C++ SDK `Counter::Add()` and `Histogram::Record()` are backed by lock-free atomics (see `opentelemetry-cpp` `UpDownCounterSyncInstrument`). They are safe to call within `ThreadLoop::process()` bodies with a measured overhead of < 50 ns per call when no context switch occurs. Only these metric instrument operations are permitted in:
- `AudioSender::update()`
- `VideoSender::encodeAndSendVideo()`
- `AudioReceiveThread` loop body
- `VideoReceiveThread::decodeFrame()`

### Acceptable span creation points (sparse — max ~4 spans per call lifetime)

| Event | Rationale |
|---|---|
| `RtpSession::start()` entry | Session lifetime start; < 1/call |
| Codec finalised (`MediaEncoder::initStream()` success) | Codec negotiation complete; < 1/call or on re-negotiation |
| `adaptQualityAndBitrate()` quality-drop event | Triggered at most once per 4-second RTCP check interval |
| `RtpSession::stop()` / destructor | Session end |
| `HardwareAccel` fallback to software | Once per session at most |

---

## Proposed Span Hierarchy (sparse — session-level only)

All media spans are children of the parent call span established by the SIP/JAMI signalling layer (see `integration_call_manager.md` when written). The call span's `TraceId` is the correlation anchor for exemplars.

```
call.invite (root span — SIP/JAMI signalling, INTERNAL)
│
├── media.session.start   (INTERNAL, child of call span)
│     Attributes:
│       jami.call.id         = <hashed call ID>            [string]
│       jami.media.type      = "audio" | "video"           [string]
│       jami.media.stream_id = <stream ID>                 [string]
│       jami.media.codec     = "opus" | "h264" | ...       [string]
│       jami.media.secure    = true | false                [bool]
│       jami.media.direction = "sendrecv"|"sendonly"|...   [string]
│
├── media.codec.negotiate  (INTERNAL, child of call span)
│     Attributes:
│       jami.media.type      = "audio" | "video"
│       jami.media.codec     = <negotiated codec name>
│       jami.media.hwaccel   = true | false
│     Events:
│       "hwaccel.fallback"   if HardwareAccel fails, adds accel_name attr
│
└── media.session.end      → recorded as an Event on the call span, NOT a separate span
      Reason: ending a span after the call span has already ended would produce
              an orphan. Use call_span->AddEvent("media.session.end", attrs) instead.
              Attrs: jami.media.type, jami.media.stream_id, total_packets_sent,
                     total_packets_received, final_packet_loss_pct
```

**Span naming convention:** `<subsystem>.<noun>.<verb>` in lowercase with dots, matching OTel semantic conventions for `db.*` and `rpc.*` namespaces.

---

## Proposed Metric Instruments

All instruments live under the `"jami.media"` meter scope (`meter = Provider::GetMeterProvider()->GetMeter("jami.media", "1.0.0")`).

Standard label set applied to **every** media metric:

| Label key | Values | Source |
|---|---|---|
| `jami.media.type` | `"audio"`, `"video"` | `RtpSession::mediaType_` |
| `jami.media.codec` | `"opus"`, `"h264"`, `"h265"`, `"vp8"`, `"vp8"`, `"g711"`, … | `MediaEncoder::getAudioCodec()` / `getVideoCodec()` |
| `jami.call.id` | hashed call ID (SHA-256 truncated to 16 hex chars) | `RtpSession::callId_` — **must be hashed, see Privacy section** |

### Instrument Table

| Name | Instrument Type | Unit | Description | Additional Labels |
|---|---|---|---|---|
| `jami.media.rtp.packets_sent` | `UInt64Counter` | `{packets}` | Total RTP packets sent over the lifetime of the session. Incremented once per `SocketPair::writeData()` call that succeeds. | — |
| `jami.media.rtp.packets_received` | `UInt64Counter` | `{packets}` | Total RTP packets received (post SRTP decrypt). Incremented in `SocketPair::readCallback()`. | — |
| `jami.media.rtp.packets_lost` | `UInt64Counter` | `{packets}` | Cumulative lost packets reported in RTCP RR (`rtcpRRHeader::cum_lost_packet`). Updated each time `AudioRtpSession::check_RCTP_Info_RR()` or the video equivalent processes an RR. **Not per-packet.** | — |
| `jami.media.rtp.jitter` | `DoubleHistogram` | `ms` | Interarrival jitter from RTCP RR (`rtcpRRHeader::jitter`, converted from RTP timestamp units using codec clock rate). Recorded every RTCP check interval (4 s). Suggested buckets: `[0, 5, 10, 20, 40, 80, 160, 320]` ms. | — |
| `jami.media.rtp.latency` | `DoubleHistogram` | `ms` | Round-trip latency from `SocketPair::getLastLatency()`. Recorded every RTCP interval. Suggested buckets: `[0, 10, 25, 50, 100, 200, 400, 800]` ms. | — |
| `jami.media.audio.bitrate` | `DoubleObservableGauge` | `By/s` | Current outgoing audio bitrate (bps). Observable callback reads `AudioSender::audioEncoder_->audioOpts_.bitrate` or `MediaEncoder::getStream()` bitrate field. Sampled at export time. | — |
| `jami.media.video.bitrate` | `DoubleObservableGauge` | `By/s` | Current outgoing video bitrate. Observable callback reads `VideoBitrateInfo::videoBitrateCurrent` from `VideoRtpSession::getVideoBitrateInfo()`. | — |
| `jami.media.video.fps` | `DoubleObservableGauge` | `{frames}/s` | Outgoing video frame rate. Observable callback reads `MediaEncoder::getStream().frameRate` or counts `VideoSender::frameNumber_` delta per second. | — |
| `jami.media.encode.duration` | `DoubleHistogram` | `ms` | Wall-clock time for one `MediaEncoder::encode()` or `MediaEncoder::encodeAudio()` call. **Critically: sampled at 1-in-N rate** (e.g., every 100th frame) to avoid per-frame overhead. Set `jami.media.sample_ratio` attribute. Suggested buckets: `[0, 0.5, 1, 2, 5, 10, 20, 50]` ms. | `jami.media.hwaccel` (`true`/`false`) |
| `jami.media.congestion.bandwidth_estimate` | `DoubleObservableGauge` | `By/s` | REMB estimated max bitrate from `CongestionControl::parseREMB()`. Updated on each REMB RTCP packet received. | — |
| `jami.media.congestion.bw_state` | `Int64ObservableGauge` | `{state}` | `BandwidthUsage` enum value (0=normal, 1=underusing, 2=overusing) from `CongestionControl::get_bw_state()`. | — |
| `jami.media.session.count` | `Int64UpDownCounter` | `{sessions}` | Number of currently active RTP sessions. `+1` in `RtpSession::start()`, `-1` in `RtpSession::stop()`. | `jami.media.type` only |

### Observable gauge registration pattern

```cpp
// In AudioRtpSession::start() or a dedicated OTelMediaInstrumentation helper:
auto audio_bitrate_gauge = meter->CreateDoubleObservableGauge(
    "jami.media.audio.bitrate", "Current outgoing audio bitrate", "By/s");

audio_bitrate_gauge->AddCallback(
    [](opentelemetry::metrics::ObserverResult result, void* state) {
        auto* session = static_cast<AudioRtpSession*>(state);
        if (session->sender_) {
            double bps = /* read from encoder opts */;
            std::map<std::string, std::string> attrs = {
                {"jami.media.type",  "audio"},
                {"jami.media.codec", session->sender_->getAudioCodec()},
                {"jami.call.id",     hashCallId(session->callId_)},
            };
            result.Observe(bps, opentelemetry::common::KeyValueIterableView(attrs));
        }
    },
    static_cast<void*>(this));
```

---

## Code Injection Points

### 1. `AudioRtpSession::processRtcpChecker()` — primary RTCP metrics emit site

**File:** [src/media/audio/audio_rtp_session.cpp](../../../src/media/audio/audio_rtp_session.cpp)
**Class:** `AudioRtpSession` | **Method:** `processRtcpChecker()`

This method runs on `rtcpCheckerThread_` every 4 seconds and already calls `check_RCTP_Info_RR()` to populate an `RTCPInfo` struct. It is the **ideal injection point** for:
- `jami.media.rtp.packets_lost` (from `rtcpRRHeader::cum_lost_packet`)
- `jami.media.rtp.jitter` (from `rtcpRRHeader::jitter`, convert using codec clock rate)
- `jami.media.rtp.latency` (from `SocketPair::getLastLatency()`)

No hot-path impact; this fires at most 0.25 Hz.

### 2. `VideoRtpSession` RTCP feedback loop

**File:** [src/media/video/video_rtp_session.cpp](../../../src/media/video/video_rtp_session.cpp)
**Class:** `VideoRtpSession` | Locate the periodic RTCP RR + REMB processing block.

Inject:
- `jami.media.rtp.jitter`, `jami.media.rtp.packets_lost` — from `getRtcpRR()`
- `jami.media.congestion.bandwidth_estimate` — from `CongestionControl::parseREMB()`
- `jami.media.congestion.bw_state` — from `CongestionControl::get_bw_state()`
- `jami.media.video.bitrate` — from `getVideoBitrateInfo().videoBitrateCurrent`

### 3. `VideoSender::encodeAndSendVideo()` — sampled encode duration

**File:** [src/media/video/video_sender.cpp](../../../src/media/video/video_sender.cpp)
**Class:** `video::VideoSender` | **Method:** `encodeAndSendVideo()`

```cpp
void VideoSender::encodeAndSendVideo(const std::shared_ptr<VideoFrame>& frame) {
    // SAMPLING GUARD — only instrument 1 in 100 frames
    static thread_local int sampleCount = 0;
    bool doMeasure = (++sampleCount % 100 == 0);

    std::chrono::steady_clock::time_point t0;
    if (doMeasure) t0 = std::chrono::steady_clock::now();

    int ret = videoEncoder_->encode(frame, forceKeyFrame_ > 0, frameNumber_++);

    if (doMeasure) {
        double ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0).count();
        // record to jami.media.encode.duration histogram
    }
    // ... existing code ...
}
```

### 4. `AudioSender::update()` — packet send counter

**File:** [src/media/audio/audio_sender.cpp](../../../src/media/audio/audio_sender.cpp)
**Class:** `AudioSender` | **Method:** `update()`

Increment `jami.media.rtp.packets_sent` counter (audio) once per successful `encodeAudio()` + `writeData()` round. The `sent_samples` counter already exists in `AudioSender`; a packet counter alongside it has zero contention risk.

### 5. `RtpSession::start()` / `stop()` — session span and session count

**File:** [src/media/rtp_session.h](../../../src/media/rtp_session.h) (base), concrete implementations
`AudioRtpSession::start()` / `VideoRtpSession::start()` / `stop()`

- Start span `media.session.start` as child of call span (via context propagation from the call manager layer).
- Increment `jami.media.session.count` +1 on start, -1 on stop.

### 6. `MediaEncoder::initStream()` — codec negotiation span

**File:** [src/media/media_encoder.cpp](../../../src/media/media_encoder.cpp)
**Class:** `MediaEncoder` | **Method:** `initStream(const SystemCodecInfo&, AVBufferRef*)`

Start span `media.codec.negotiate` here. Record codec name (`videoCodec_` / `audioCodec_` assigned during `initStream`), hardware acceleration status (`enableAccel_`, `accel_` pointer non-null), and whether HW init succeeded or fell back to SW.

### 7. `SocketPair` packet received counter

**File:** [src/media/socket_pair.cpp](../../../src/media/socket_pair.cpp)
**Class:** `SocketPair` | **Method:** `readCallback()` or `readRtpData()`

Increment `jami.media.rtp.packets_received` once per successfully decrypted RTP packet. Using an `std::atomic<uint64_t>` member in `SocketPair` and periodically snapshotting it into the OTel counter avoids per-packet SDK overhead. Snapshot in the RTCP checker thread (4 s interval).

---

## Exemplars Strategy

OTel exemplars allow a metric data point to embed a `{trace_id, span_id, value}` tuple, linking a histogram bucket boundary event back to the live span at that moment.

**Use case:** `jami.media.rtp.jitter` histogram — if an observation falls into the ≥ 80 ms bucket (the anomalous tail), an exemplar recording the `media.session.start` span's `trace_id` lets an operator jump from the metric anomaly directly to the call trace in Jaeger/Tempo.

**Implementation approach:**

```cpp
// Inside processRtcpChecker() when recording jitter:
auto ctx = opentelemetry::context::RuntimeContext::GetCurrent();
auto span = opentelemetry::trace::GetSpan(ctx);  // the media.session.start span kept in context

if (jitter_ms >= 80.0) {
    // Record with exemplar context — SDK auto-attaches trace_id/span_id
    // when the current context carries an active span.
    jitter_histogram->Record(jitter_ms, kv, ctx);
} else {
    jitter_histogram->Record(jitter_ms, kv);
}
```

The RTCP checker thread must have the `media.session.start` span's context propagated into it when `rtcpCheckerThread_` is started (store it as a `opentelemetry::context::Context` member of `AudioRtpSession` at `start()` time and activate it in the checker thread's setup lambda).

**Recommended exemplar filter:** `AlwaysOnExemplarFilter` for `jitter` and `encode.duration` histograms; `TracingExemplarFilter` (only records when trace is sampled) for all others.

---

## Privacy Considerations

### Call ID hashing

`RtpSession::callId_` and `RtpSession::streamId_` are platform-assigned identifiers. They **must not** appear as raw label values in metric exports because:
- In JAMI P2P calls, the call ID may be derived from or correlated with DHT keys that could be used to infer account identity.
- Raw IDs in time-series labels create high-cardinality dimensions that cause metric storage explosion in Prometheus/VictoriaMetrics (one time-series per unique call ID × codec × media type).

**Required transformation:** hash call IDs before use as labels using SHA-256 truncated to 64 bits (16 hex characters):

```cpp
static std::string hashCallId(const std::string& callId) {
    // Use existing jami::base64 + crypto utilities or std::hash for low-security label use
    // For production: SHA-256 via gnutls_hash_fast(GNUTLS_DIG_SHA256, ...)
    // truncated to first 8 bytes, formatted as hex
}
```

Alternatively, use a **session-local opaque integer** (e.g., incrementing `uint64_t` assigned in `RtpSession` constructor) as the label, which provides correlation within a single daemon lifetime without leaking any identifier.

### Peer address exclusion

`RtpSession::getSendAddr()` and `getRecvAddr()` return `dhtnet::IpAddr` peer endpoints. These **must never** appear in metric labels or span attributes. Peer IPs are PII in jurisdictions with strict privacy regulations (GDPR, Quebec Law 25). If endpoint-level diagnostics are needed, use the hashed call ID to correlate to a trace where the address can be stored under access-controlled log sinks rather than in the public metrics pipeline.

### Codec metadata is safe

`MediaEncoder::getAudioCodec()` / `getVideoCodec()` return strings like `"opus"`, `"h264"` — these are non-identifying and safe as label values.

---

## Thread Safety Notes

1. **Metric instrument objects** (`Counter`, `Histogram`, `ObservableGauge`) returned by `GetMeter()->Create*()` are thread-safe for concurrent `Add()` / `Record()` calls from different `ThreadLoop` threads. The OTel C++ SDK guarantees this.

2. **Observable gauge callbacks** are invoked on the **exporter/reader thread**, not on any `ThreadLoop`. The callback lambda must therefore access `AudioRtpSession` / `VideoRtpSession` state through thread-safe accessors (e.g., read `VideoBitrateInfo` under `VideoRtpSession::mutex_`). Do not access `sender_` or `receiveThread_` raw pointers from the callback without the session mutex — they may be null during `stop()`.

3. **Sampling atomic in hot path:** The `thread_local int sampleCount` pattern for `jami.media.encode.duration` uses thread-local storage, which is zero-cost after the first access. No shared state; fully safe in the sender `ThreadLoop`.

4. **RTCP checker thread context:** When propagating the `media.session.start` span context into `rtcpCheckerThread_`, capture the `Context` by value in the `InterruptedThreadLoop` setup lambda (not by reference to `AudioRtpSession`) to avoid use-after-free if `stop()` is called while the checker is mid-execution.

5. **`SocketPair` read-path counter:** Prefer an `std::atomic<uint64_t>` inside `SocketPair` for raw packet counts and snapshot it from the RTCP checker thread rather than calling the OTel SDK directly from the `readCallback()` path, which may run under `dataBuffMutex_`.

---

## Risks & Complications

| Risk | Severity | Mitigation |
|---|---|---|
| High cardinality from `jami.call.id` label | High | Hash + truncate to 64-bit; or use session-local opaque integer |
| Per-frame sampling complexity (`encode.duration`) | Medium | Thread-local counter; 1-in-100 sampling; document sample ratio as attribute |
| RTCP data not always available (early call, muted peer) | Medium | Guard metric records behind null checks on `getRtcpRR()` list; use NaN / skip instead of emitting 0 |
| Observable gauge callbacks racing with `stop()` | High | Deregister callbacks in `stop()` before destroying `sender_` and `receiveThread_`; use weak_ptr or flag |
| `MediaEncoder` accessed from both sender thread and RTCP checker | Medium | `MediaEncoder::encMutex_` already guards `setBitrate()`; metric reads of `getAudioCodec()` / `getVideoCodec()` (return `const std::string&`) are safe without locking if read after initial codec setup |
| HardwareAccel path variability (VAAPI vs. SW) | Low | Tag `jami.media.hwaccel` label on `encode.duration` histogram; keeps distributions separable |
| `rtcpRRHeader::jitter` in clock-rate units, not ms | Medium | Convert: `jitter_ms = jitter_units * 1000.0 / clock_rate_hz`. Audio (Opus): 48000 Hz. Video (H.264): 90000 Hz |
| Multiple concurrent calls = multiple sessions sharing one `Meter` | Low | Labels differentiate by hashed call ID and media type; correct OTel multi-instance behaviour |
| `InterruptedThreadLoop` for RTCP checker may skip cycles | Low | Acceptable — 4 s interval is coarse enough that skipped cycles are cosmetic |

---

## Source References

| Class / File | Relevance |
|---|---|
| [src/media/rtp_session.h](../../../src/media/rtp_session.h) | Base class: `callId_`, `streamId_`, `mediaType_`, `start()`, `stop()` |
| [src/media/audio/audio_rtp_session.h](../../../src/media/audio/audio_rtp_session.h) | `RTCPInfo`, `processRtcpChecker()`, `rtcpCheckerThread_`, `adaptQualityAndBitrate()` |
| [src/media/video/video_rtp_session.h](../../../src/media/video/video_rtp_session.h) | `RTCPInfo`, `VideoBitrateInfo`, `getVideoBitrateInfo()` |
| [src/media/socket_pair.h](../../../src/media/socket_pair.h) | `getRtcpRR()`, `getRtcpREMB()`, `getLastLatency()`, `lastSeqValOut()`, `rtcpRRHeader`, `rtcpREMBHeader` |
| [src/media/media_encoder.h](../../../src/media/media_encoder.h) | `encode()`, `encodeAudio()`, `setBitrate()`, `setPacketLoss()`, `getAudioCodec()`, `getVideoCodec()`, `encMutex_` |
| [src/media/media_decoder.h](../../../src/media/media_decoder.h) | `MediaDecoder::decode()`, `MediaDemuxer`, `DecodeStatus` enum |
| [src/media/audio/audio_sender.h](../../../src/media/audio/audio_sender.h) | `AudioSender::update()`, `sent_samples` |
| [src/media/video/video_sender.h](../../../src/media/video/video_sender.h) | `VideoSender::encodeAndSendVideo()`, `frameNumber_`, `setBitrate()` |
| [src/media/audio/audio_receive_thread.h](../../../src/media/audio/audio_receive_thread.h) | `AudioReceiveThread`, `ThreadLoop` usage |
| [src/media/video/video_receive_thread.h](../../../src/media/video/video_receive_thread.h) | `VideoReceiveThread::decodeFrame()`, `startLoop()` / `stopLoop()` |
| [src/media/congestion_control.h](../../../src/media/congestion_control.h) | `CongestionControl::parseREMB()`, `kalmanFilter()`, `get_bw_state()`, `BandwidthUsage` enum |
| [src/media/media_attribute.h](../../../src/media/media_attribute.h) | `MediaAttribute::secure_`, `type_`, `muted_` — session-start span attributes |
| [src/media/video/video_rtp_session.h](../../../src/media/video/video_rtp_session.h) | `VideoBitrateInfo` struct with `videoBitrateCurrent`, `packetLostThreshold` |

---

## Open Questions

1. **Context propagation into RTCP checker thread:** How should the `media.session.start` span context be propagated into `rtcpCheckerThread_`? The `InterruptedThreadLoop` setup lambda could capture a `Context` snapshot from `start()` — confirm this is the intended pattern with the OTel integration layer.

2. **Sampling rate for `encode.duration`:** Is 1-in-100 frames the right trade-off? For a 30 fps video session this produces one sample every ~3.3 seconds, which may be too coarse to catch transient encode spikes during congestion events. Consider 1-in-30 (one per second) or adaptive sampling triggered by RTCP loss events.

3. **Observable gauge deregistration API:** The OTel C++ SDK's `RemoveCallback()` is required in `stop()` to prevent stale callbacks reading destroyed session state. Confirm the SDK version used in jami-daemon exposes `RemoveCallback()` on observable instruments (added in opentelemetry-cpp 1.9).

4. **RTCP clock-rate for jitter conversion:** Audio sessions use Opus at 48000 Hz; video sessions use H.264/H.265/VP8 at 90000 Hz. Is the clock rate reliably available from `MediaDecoder::getStream().sampleRate` at the point RTCP is processed, or does it need to be stashed during codec negotiation?

5. **`jami.media.rtp.packets_sent` vs. `MediaEncoder::getLastSeqValue()`:** Should packet-sent counting use `SocketPair::lastSeqValOut()` (sequence number, 16-bit wrapping) or a separate monotonic counter? Sequence number wrapping at 65535 makes it unsuitable as a raw counter value; a separate `std::atomic<uint64_t>` is cleaner.

6. **Multi-stream sessions:** A single call can carry multiple `RtpSession` instances (audio + video, or multiple video streams in conference). Confirm that the `jami.media.stream_id` label (from `RtpSession::streamId_`) sufficiently disambiguates streams within the same call when querying metrics.

7. **Is `webrtc-audio-processing` instrumentation in scope?** The `AudioProcessor` in `src/media/audio/audio-processing/` runs AEC and noise suppression. VAD (voice activity detection) state is already propagated via `AudioSender::voiceCallback_`. Should AEC residual error or noise suppression gain be exposed as metrics?
