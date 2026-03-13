# Layer 4 — Data Plane / Media Pipeline

## Status: draft

## Last Updated: 2026-03-13

---

## Layer Description

Layer 4 is the **real-time data path**: every audio sample and video frame that flows between peers passes through this layer. It begins at the hardware audio capture callback or camera frame arrival, proceeds through encoding, RTP/SRTP packetisation, transmission, reception, SRTP decryption, decoding, and terminates at audio playback or the shared-memory video sink. It is the highest-CPU subsystem and the only one where OTel instrumentation decisions have a direct, measurable impact on call quality.

### Constituting Files and Classes

#### Session Management

| Class | File | Role |
|---|---|---|
| `RtpSession` | `src/media/rtp_session.h` | Abstract base: `start(IceSocket rtp, IceSocket rtcp)`, `stop()`, `setMuted()` |
| `AudioRtpSession` | `src/media/audio/audio_rtp_session.h` / `.cpp` | Orchestrates audio send+receive threads; owns `AudioSender`, `AudioReceiveThread`; RTCP check loop every 4 s |
| `VideoRtpSession` | `src/media/video/video_rtp_session.h` / `.cpp` | Orchestrates video send+receive; owns `VideoSender`, `VideoReceiveThread`; bandwidth estimation via `CongestionControl` |
| `SocketPair` | `src/media/socket_pair.h` / `.cpp` | RTP+RTCP transport over ICE socket; SRTP encrypt/decrypt via libsrtp; provides `writeData()` and `readCallback()` |

#### Encoding / Decoding Hot Path

| Class | File | Role |
|---|---|---|
| `MediaEncoder` | `src/media/media_encoder.h` / `.cpp` | FFmpeg `AVCodecContext` encode wrapper; `encode(VideoFrame)`, `encodeAudio()`; supports VAAPI/VideoToolbox/MediaCodec |
| `MediaDecoder` | `src/media/media_decoder.h` / `.cpp` | FFmpeg decode wrapper; `decode()` returns `MediaDecoder::Status` |
| `AudioSender` | `src/media/audio/audio_sender.h` / `.cpp` | Reads from `RingBuffer`; calls `MediaEncoder::encodeAudio()`; packetises via `AVFormatContext` mux |
| `VideoSender` | `src/media/video/video_sender.h` / `.cpp` | Reads from `VideoInput` frame queue; calls `MediaEncoder::encode()`; `encodeAndSendVideo()` is the hot-path method |
| `AudioReceiveThread` | `src/media/audio/audio_receive_thread.h` / `.cpp` | `ThreadLoop` running `MediaDecoder::decode()` → `RingBuffer::put()` |
| `VideoReceiveThread` | `src/media/video/video_receive_thread.h` / `.cpp` | `ThreadLoop` running `decodeFrame()` → `SinkClient` notify |

#### Capture / Render

| Class | File | Role |
|---|---|---|
| `AudioLayer` | `src/media/audio/audiolayer.h` / `.cpp` | Abstract HW I/O; hardware callback thread; backends: PulseAudio, ALSA, CoreAudio, AAudio, JACK, PortAudio |
| `RingBufferPool` | `src/media/audio/ringbufferpool.h` / `.cpp` | Named lock-free ring buffer registry; routes audio between calls, ringtones, conferencing |
| `VideoInput` | `src/media/video/video_input.h` / `.cpp` | V4L2 / screen / file capture; frame-passive-reader observer chain |
| `SinkClient` | `src/media/video/sinkclient.h` / `.cpp` | Shared-memory video sink to UI; `shm_header.h` layout |

#### Quality / Congestion

| Class | File | Role |
|---|---|---|
| `CongestionControl` | `src/media/congestion_control.h` / `.cpp` | REMB/TWCC bandwidth estimation; Kalman filter; `setBitrate()` feedback to `MediaEncoder` |
| `AudioProcessor` | `src/media/audio/audio-processing/` | AEC, noise suppression via Speex/WebRTC |
| `MediaFilter` | `src/media/media_filter.h` / `.cpp` | FFmpeg `libavfilter` graph (scale, transpose, watermark) |
| `VideoMixer` | `src/media/video/video_mixer.h` / `.cpp` | Conference video compositor; own `ThreadLoop` |

#### Threading Model Summary

| Thread | Owner | Instrumentation restriction |
|---|---|---|
| `AudioLayer` hardware callback | OS audio subsystem | **FORBIDDEN: no alloc, no span** |
| `AudioSender` `ThreadLoop::process()` | `AudioRtpSession` | Lock-free Counter/Histogram Record only |
| `VideoSender` `ThreadLoop::process()` | `VideoRtpSession` | Lock-free Counter/Histogram Record only |
| `AudioReceiveThread` `ThreadLoop::process()` | `AudioRtpSession` | Lock-free Counter/Histogram Record only |
| `VideoReceiveThread` `ThreadLoop::process()` | `VideoRtpSession` | Lock-free Counter/Histogram Record only |
| `AudioRtpSession::rtcpCheckerThread_` | `AudioRtpSession` | Histograms and occasional span events OK (4 s period) |
| `VideoRtpSession` RTCP / REMB callback | Async callback | Histograms OK; spans only for adapt events |
| `RtpSession::start()` / `stop()` | io_context | Full span creation OK |

---

## OTel Relevance

Layer 4 is the **busiest layer in the daemon** and the layer where instrumentation mistakes have the most severe consequences:

- At 50 audio frames/second per active call, a single `StartSpan()` per packet at 10 concurrent calls = **500 span allocations/second** — enough to cause measurable encode jitter.
- PER-PACKET instrumentation is **categorically forbidden**. The value of per-packet data does not justify the cost; aggregate rates from RTCP RR already provide packet loss %, jitter, and RTT with zero overhead.
- **Session-level events** (RTP session start/stop, codec finalisation, quality adaptation) are appropriate for spans because they occur at most a handful of times per call lifetime.
- **Quality metrics** (bitrate, frame rate, jitter, packet loss) should be reported as OTel metrics using `ObservableGauge` (pulled at the export interval) or `Histogram` (recorded at the 4-second RTCP interval), not as spans.

---

## CRITICAL: Hot Path Policy

> **RULE**: No `StartSpan()`, no `EndSpan()`, no `AddEvent()`, and no heap allocation inside any of the following methods. Violations will cause audio glitches and video frame drops.

| Prohibited location | Reason |
|---|---|
| `ThreadLoop::process()` body (all variants) | Real-time audio/video loop; O(50–100 Hz) |
| `AudioSender::update()` | Called for every encoded audio frame |
| `VideoSender::encodeAndSendVideo()` | Called for every encoded video frame |
| `AudioReceiveThread` decode body | RTP receive decode loop |
| `VideoReceiveThread::decodeFrame()` | RTP receive decode loop |
| `SocketPair::readCallback()` | Per-packet receive callback |
| `SocketPair::writeData()` | Per-packet send callback |
| `AudioLayer` hardware I/O callback | OS real-time constraint |

**These methods MAY call** (lock-free, < 50 ns):
- `Counter::Add(1, NoAttrKVIterable{})` — atomic increment, no allocation
- `Histogram::Record(value, NoAttrKVIterable{})` — atomic-based fast path when attributes are pre-computed

**`thread_local` Decimation Pattern** — for encode latency sampling (approved):

```cpp
// In VideoSender::encodeAndSendVideo() — measure 1 in 100 frames only
thread_local int frameCounter = 0;
if (++frameCounter % 100 == 0) {
    auto t0 = std::chrono::steady_clock::now();
    auto result = encoder_->encode(frame);
    auto elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0).count();
    // No attributes per invocation — pre-declare static attrs
    encodeDurationHist_.Record(elapsed_ms, staticMediaAttrs_);
} else {
    encoder_->encode(frame);
}
```

---

## Recommended Signal Mix

| Signal | Instruments | Notes |
|---|---|---|
| **Metrics** (primary) | `ObservableGauge` for bitrate/FPS/congestion state; `Histogram` recorded at RTCP interval (4 s) for jitter/latency/packet loss; `UpDownCounter` for active session count | All metrics, no real-time tracing |
| **Traces** (sparse) | `media.session.start`, `media.codec.negotiate` (at session init); quality-adapt event (at RTCP check) as a span event; `media.session.end` recorded as event on call span | Max ~4 spans per call lifetime |
| **Logs** | Bridge critical errors (`MediaEncoder::encode()` returning `AVERROR`, SRTP failure) to OTel Log Bridge at `kError` | Already covered by `JAMI_ERR` macros; bridge them |

---

## Instrumentation Instruments — Full Table

All instruments belong to the `"jami.media"` meter scope.

**Standard label set** (applied to every instrument unless noted):

| Label key | Allowed values |
|---|---|
| `jami.media.type` | `"audio"`, `"video"` |
| `jami.media.codec` | `"opus"`, `"h264"`, `"h265"`, `"vp8"`, `"g711a"`, `"g711u"` — bounded set from codec registry |

> `jami.call.id` (hashed) may appear as a **span attribute** but is **FORBIDDEN as a metric label** due to high cardinality.

| Instrument Name | Type | Unit | Description |
|---|---|---|---|
| `jami.media.rtp.packets_sent` | `UInt64Counter` | `{packets}` | Total RTP packets sent; incremented in `SocketPair::writeData()` success path |
| `jami.media.rtp.packets_received` | `UInt64Counter` | `{packets}` | Total RTP packets received post SRTP decrypt |
| `jami.media.rtp.packets_lost` | `UInt64Counter` | `{packets}` | Cumulative lost packets from RTCP RR `cum_lost_packet`; updated at RTCP interval |
| `jami.media.rtp.jitter` | `DoubleHistogram` | `ms` | Interarrival jitter from RTCP RR; recorded every 4 s. Buckets: `[0,5,10,20,40,80,160,320]` ms |
| `jami.media.rtp.latency` | `DoubleHistogram` | `ms` | RTT from `SocketPair::getLastLatency()`; recorded every RTCP interval. Buckets: `[0,10,25,50,100,200,400,800]` ms |
| `jami.media.audio.bitrate` | `DoubleObservableGauge` | `By/s` | Current outgoing audio bitrate; polled from `AudioSender` encoder opts at export time |
| `jami.media.video.bitrate` | `DoubleObservableGauge` | `By/s` | Current outgoing video bitrate; polled from `VideoRtpSession::getVideoBitrateInfo()` |
| `jami.media.video.fps` | `DoubleObservableGauge` | `{frames}/s` | Outgoing video frame rate; polled from encoder stream info |
| `jami.media.encode.duration` | `DoubleHistogram` | `ms` | Encode wall-clock time (1-in-100 sampled); `jami.media.hwaccel` (bool) label added. Buckets: `[0,0.5,1,2,5,10,20,50]` ms |
| `jami.media.congestion.bandwidth_estimate` | `DoubleObservableGauge` | `By/s` | REMB estimated max bitrate from `CongestionControl::parseREMB()` |
| `jami.media.congestion.bw_state` | `Int64ObservableGauge` | `{state}` | Bandwidth usage state: 0=normal, 1=underuse, 2=overuse |
| `jami.media.session.count` | `Int64UpDownCounter` | `{sessions}` | Active RTP sessions; `+1` in `RtpSession::start()`, `-1` in `stop()`; `jami.media.type` label only |

---

## Example C++ Instrumentation Snippet

### ObservableGauge Registration for Video Bitrate

```cpp
// src/media/video/video_rtp_session.cpp
#include "opentelemetry/metrics/provider.h"

namespace metric_api = opentelemetry::metrics;

void VideoRtpSession::start(std::unique_ptr<IceSocket> rtp,
                            std::unique_ptr<IceSocket> rtcp)
{
    // 1. Session-level span (OK — called rarely, not in hot path)
    auto tracer = opentelemetry::trace::Provider::GetTracerProvider()
                      ->GetTracer("jami.media", "1.0.0");
    auto scope_token = /* attach parent call span context */;
    auto sessionSpan = tracer->StartSpan("media.session.start");
    sessionSpan->SetAttribute("jami.media.type",   std::string("video"));
    sessionSpan->SetAttribute("jami.media.codec",  getVideoCodecName());
    sessionSpan->SetAttribute("jami.media.secure", isSrtpEnabled());
    sessionSpan->SetStatus(opentelemetry::trace::StatusCode::kOk);
    sessionSpan->End();

    // 2. Register ObservableGauge — called once; safe here
    auto meter = metric_api::Provider::GetMeterProvider()
                     ->GetMeter("jami.media", "1.0.0");

    videoBitrateGauge_ = meter->CreateDoubleObservableGauge(
        "jami.media.video.bitrate",
        "Current outgoing video bitrate",
        "By/s");

    // State pointer must outlive the callback; 'this' is safe because
    // the gauge is destroyed in stop() before 'this' is destructed.
    videoBitrateGauge_->AddCallback(
        [](opentelemetry::metrics::ObserverResult result, void* state) {
            auto* session = static_cast<VideoRtpSession*>(state);
            auto info = session->getVideoBitrateInfo();
            // Codec name is bounded; safe as an observable attribute
            std::map<std::string, std::string> attrs = {
                {"jami.media.type",  "video"},
                {"jami.media.codec", session->getVideoCodecName()},
            };
            result.Observe(
                static_cast<double>(info.videoBitrateCurrent),
                opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs});
        },
        static_cast<void*>(this));

    // 3. UpDownCounter — session started
    std::map<std::string, std::string> attrs = {{"jami.media.type", "video"}};
    sessionCountInstrument_.Add(+1,
        opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs});

    // 4. Existing setup logic follows
    RtpSession::start(std::move(rtp), std::move(rtcp));
}
```

### Sampled Histogram for Encode Latency with thread_local Decimation

```cpp
// src/media/video/video_sender.cpp
// Pre-declare attributes statically to avoid map allocation on hot path
namespace {
    struct StaticVideoAttrs {
        std::string type  = "video";
        std::string codec;  // set once in sender init
    };
}

void VideoSender::encodeAndSendVideo()
{
    // ── Decimated encode latency measurement ─────────────────────────────────
    thread_local int sampleCounter = 0;
    constexpr int SAMPLE_EVERY_N = 100;    // sample 1% of frames

    if (++sampleCounter >= SAMPLE_EVERY_N) {
        sampleCounter = 0;

        auto t_start = std::chrono::steady_clock::now();

        // ... existing encode logic here ...
        int ret = encoder_->encode(currentFrame_);

        double elapsed_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t_start).count();

        // encodeDurationHist_ cached as member; no allocation
        // staticEncoderAttrs_ pre-built and stored as member in start()
        encodeDurationHist_->Record(elapsed_ms, staticEncoderAttrsView_);
    } else {
        // Normal (non-sampled) path — no timing overhead
        encoder_->encode(currentFrame_);
    }

    // ── Lock-free packet counter update ──────────────────────────────────────
    // packetssentCounter_ is a UInt64Counter with pre-built NoAttrKVIterable
    packetsSentCounter_->Add(1, staticEncoderAttrsView_);
}
```

### RTCP Interval Histogram Recording (approved — 4 s period, not hot path)

```cpp
// src/media/audio/audio_rtp_session.cpp — processRtcpChecker()
void AudioRtpSession::processRtcpChecker()
{
    auto rr = getRtcpRR();
    if (!rr) return;

    // Convert RTP jitter timestamp units to milliseconds
    double jitter_ms = static_cast<double>(rr->jitter) * 1000.0
                       / static_cast<double>(clockRate_);

    std::map<std::string, std::string> attrs = {
        {"jami.media.type",  "audio"},
        {"jami.media.codec", getAudioCodecName()},
    };
    auto kv = opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs};

    jitterHistogram_->Record(jitter_ms, kv);

    double latency_ms = getLastLatency();
    if (latency_ms > 0.0)
        latencyHistogram_->Record(latency_ms, kv);

    // Cumulative packet loss from RR
    if (rr->cum_lost_packet > lastReportedLoss_) {
        uint64_t delta = rr->cum_lost_packet - lastReportedLoss_;
        packetsLostCounter_->Add(delta, kv);
        lastReportedLoss_ = rr->cum_lost_packet;
    }

    // Quality-adapt event — create a span EVENT (not a span) on the call span
    // This is safe here — 4 s period, not the encode loop
    if (shouldAdaptQuality()) {
        adaptQualityAndBitrate();
        // Add a timestamped event to the live call span rather than
        // opening a new child span (avoids span explosion in long calls)
        if (callSpanRef_) {
            callSpanRef_->AddEvent("media.audio.quality_adapt", {
                {"jami.media.type",          std::string("audio")},
                {"jami.media.codec",         getAudioCodecName()},
                {"jami.media.adapt.reason",  std::string("rtcp_feedback")},
            });
        }
    }
}
```

---

## Exemplars — Linking a Metric Spike to a Trace Span

OTel Exemplars allow a metric data point to carry a `(trace_id, span_id)` reference, enabling an operator to jump from a high-jitter histogram bucket directly to the trace for that call.

### How Exemplars Work in This Layer

When `jitter_histogram_->Record(jitter_ms, attrs)` is called and the current thread has an **active span** in its context, the OTel SDK may automatically attach the span context as an exemplar (controlled by the `AlwaysOffExemplarFilter` / `TraceBased` / `AlwaysOn` filter configured in the `MeterProvider`).

For this to work in the media pipeline:
1. The RTCP checker thread must have the **call span context attached** before calling `Record()`.
2. This requires storing the `callSpanContext_` (obtained at `RtpSession::start()`, as shown in Layer 3) and attaching it at the start of `processRtcpChecker()`:

```cpp
void AudioRtpSession::processRtcpChecker()
{
    // Attach the call span context so SDK can attach exemplars
    auto token = opentelemetry::context::RuntimeContext::Attach(callSpanContext_);

    // ... histogram Record() calls below will now carry exemplar trace_id/span_id ...
    jitterHistogram_->Record(jitter_ms, kv);
}
```

3. Configure the `MeterProvider` with `TraceBasedExemplarFilter` (the default in recent SDK versions) so exemplars are only attached when the current span is being sampled.

**Result**: In an observability backend (Grafana / Jaeger), clicking on a jitter spike in the metrics timeline will navigate directly to the trace for that call, showing exactly which ICE path was in use and whether a quality adaptation event occurred.

---

## Cardinality Warnings

| ⚠️ DO NOT | Reason |
|---|---|
| Use `callId` (even hashed) as a metric label on any media instrument | One label value per active call × label cardinality = unbounded growth in metrics storage |
| Use `rtpSsrc` or `rtpPt` (payload type number) as a metric label | High cardinality (unique per call) |
| Use peer IP address or port as any telemetry field at metric level | PII; unbounded |
| Create per-stream histograms with stream-unique labels | Instead, separate streams by `jami.media.type` and `jami.media.codec` — both bounded |
| Add codec string to labels without bounding | Only values from the codec registry (`SystemCodecContainer`) are permitted; log an error and skip if an unknown codec name is observed |

---

## Subsystems in This Layer

| Subsystem | Relationship |
|---|---|
| **media_pipeline** | This layer is a direct description of `media_pipeline`; all classes listed above are from this subsystem |
| **conference** | `VideoMixer` and `AudioLayer` ring buffer routing are used for conference mixing; conference audio/video flows through this layer with the same hot-path restrictions |
| **data_transfer** | File transfer uses a separate dhtnet channel, not RTP; Layer 4 instrumentation does not apply to data transfer |
| **plugin_system** | `CallServicesManager` injects media hooks synchronously into the encode/decode `ThreadLoop`; plugin invocations must follow the same hot-path restriction (no `StartSpan`) |
| **signaling_control** (Layer 3) | The `call.media.start` span (Layer 3) is the parent of `media.session.start` (Layer 4); the call span context flows downward |

---

## Source References

- `src/media/rtp_session.h`
- `src/media/audio/audio_rtp_session.h` / `.cpp`
- `src/media/video/video_rtp_session.h` / `.cpp`
- `src/media/socket_pair.h` / `.cpp`
- `src/media/media_encoder.h` / `.cpp`
- `src/media/media_decoder.h` / `.cpp`
- `src/media/audio/audio_sender.h` / `.cpp`
- `src/media/video/video_sender.h` / `.cpp` — `encodeAndSendVideo()` is the key hot-path method
- `src/media/congestion_control.h` / `.cpp`
- `src/media/video/video_mixer.h` / `.cpp`
- `src/media/audio/ringbufferpool.h`
- `src/threadloop.h`
- KB: `subsystem_media_pipeline.md` — full pipeline stages diagram
- KB: `integration_media_pipeline.md` — complete metric instrument table and observable gauge pattern
- KB: `otel_metrics.md` — ObservableGauge, Counter, Histogram API
- KB: `otel_traces.md` — span context propagation for exemplars

---

## Open Questions

1. **Observable gauge lifetime**: `ObservableGauge` objects that hold a `this` pointer (e.g., `videoBitrateGauge_`) must be destroyed in `RtpSession::stop()` before the `VideoRtpSession` is destructed. Confirm the teardown order and add a `videoBitrateGauge_.reset()` to `stop()`.
2. **callSpanContext_ propagation**: the RTCP checker thread is a private `std::thread` inside `AudioRtpSession`. It must be given the `callSpanContext_` at `start()` time. Confirm there is no race between `start()` (which sets the context) and the first RTCP check (which uses it).
3. **Codec label boundary**: `MediaEncoder` can theoretically be configured with any codec in the FFmpeg registry. The metric attribute restriction to `SystemCodecContainer` values must be enforced at the recording site. Define a `isBoundedCodecLabel(std::string_view)` predicate.
4. **SRTP error counters**: `SocketPair` silently drops SRTP decrypt failures today. These should increment a `jami.media.srtp.errors` counter (bounded by `error.type = "decrypt_fail"` / `"replay_check_fail"`) without creating a span.
5. **Hardware acceleration exemplars**: when `HardwareAccel` falls back to software encoding, this is a critical event. Should it add an event to the call span OR emit a dedicated Metrics data point via a `jami.media.hwaccel.fallbacks` Counter?
