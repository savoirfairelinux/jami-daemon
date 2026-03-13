# Media Pipeline

## Status: draft

## Last Updated: 2026-03-13

---

## Purpose

The media pipeline subsystem handles all real-time audio and video processing within libjami. It encompasses hardware audio I/O (via platform-specific AudioLayer backends), ring buffer mixing, audio resampling and echo cancellation, RTP/SRTP session management, FFmpeg-based encoding and decoding for audio and video codecs, RTP socket pairing over ICE channels, video device input, video mixing for conferences, local and cloud recording, and media filtering via FFmpeg `libavfilter`. It is the highest-CPU subsystem and runs continuously during active calls.

---

## Key Files

**Audio:**
- `src/media/audio/audiolayer.h` / `.cpp` — `AudioLayer` abstract hardware I/O base
- `src/media/audio/ringbuffer.h` / `.cpp` — `RingBuffer` (lock-free circular audio buffer)
- `src/media/audio/ringbufferpool.h` / `.cpp` — `RingBufferPool` (named buffer registry, owned by `Manager`)
- `src/media/audio/audio_rtp_session.h` / `.cpp` — `AudioRtpSession` (audio send/receive loop)
- `src/media/audio/audio_input.h` / `.cpp` — `AudioInput` (reads from ring buffer or file)
- `src/media/audio/audio_sender.h` / `.cpp` — `AudioSender` (encode + RTP packetize)
- `src/media/audio/audio_receive_thread.h` / `.cpp` — `AudioReceiveThread` (RTP receive + decode)
- `src/media/audio/resampler.h` / `.cpp` — `Resampler` (libswresample wrapper)
- `src/media/audio/audio-processing/` — `AudioProcessor` (AEC, noise suppression via Speex/WebRTC)

**Video:**
- `src/media/video/video_rtp_session.h` / `.cpp` — `VideoRtpSession`
- `src/media/video/video_input.h` / `.cpp` — `VideoInput` (V4L2, camera, screen, file)
- `src/media/video/video_sender.h` / `.cpp` — `VideoSender`
- `src/media/video/video_receive_thread.h` / `.cpp` — `VideoReceiveThread`
- `src/media/video/video_mixer.h` / `.cpp` — `VideoMixer` (conference compositor)
- `src/media/video/sinkclient.h` / `.cpp` — `SinkClient` (shared-memory video sink to UI)
- `src/media/video/accel.h` / `.cpp` — hardware acceleration (VAAPI, VideoToolbox, MediaCodec)
- `src/media/video/video_device_monitor.h` / `.cpp` — `VideoDeviceMonitor`

**Common:**
- `src/media/media_encoder.h` / `.cpp` — `MediaEncoder` (FFmpeg AVCodecContext encode wrapper)
- `src/media/media_decoder.h` / `.cpp` — `MediaDecoder` (FFmpeg decode wrapper)
- `src/media/media_filter.h` / `.cpp` — `MediaFilter` (FFmpeg `libavfilter` graph)
- `src/media/media_codec.h` / `.cpp` — `SystemCodecInfo`, `AccountCodecInfo` codec registry
- `src/media/media_buffer.h` / `.cpp` — `MediaFrame` (wraps `AVFrame`)
- `src/media/rtp_session.h` — `RtpSession` abstract base
- `src/media/socket_pair.h` / `.cpp` — `SocketPair` (RTP+RTCP transport over UDP or ICE socket)
- `src/media/srtp.h` / `srtp.c` — SRTP encrypt/decrypt shim over libsrtp
- `src/media/media_recorder.h` / `.cpp` — `MediaRecorder` (local recording to file)
- `src/media/media_player.h` / `.cpp` — `MediaPlayer` (file playback)
- `src/media/congestion_control.h` / `.cpp` — `CongestionControl` (REMB/TWCC feedback)
- `src/media/system_codec_container.h` / `.cpp` — `SystemCodecContainer` (codec capability probe at startup)

---

## Key Classes

| Class | Role | File |
|---|---|---|
| `AudioLayer` | Abstract HW audio I/O; concrete implementations: PulseAudio, ALSA, CoreAudio, AAudio, JACK, PortAudio | `src/media/audio/audiolayer.h` |
| `RingBufferPool` | Named map of `RingBuffer`s; routes audio between calls, ringtones, and conferencing | `src/media/audio/ringbufferpool.h` |
| `AudioRtpSession` | Orchestrates audio send + receive threads; owns `AudioSender` and `AudioReceiveThread` | `src/media/audio/audio_rtp_session.h` |
| `VideoRtpSession` | Orchestrates video send + receive threads; owns `VideoSender` and `VideoReceiveThread` | `src/media/video/video_rtp_session.h` |
| `RtpSession` | Abstract base: `start(IceSocket rtp, IceSocket rtcp)`, `stop()`, `setMuted()` | `src/media/rtp_session.h` |
| `SocketPair` | Binds RTP + RTCP sockets; SRTP encrypt/decrypt; RTCP multiplexing | `src/media/socket_pair.h` |
| `MediaEncoder` | Wraps AVCodecContext; inputs `AVFrame`, outputs `AVPacket`; supports hardware acceleration | `src/media/media_encoder.h` |
| `MediaDecoder` | Wraps AVCodecContext; inputs `AVPacket`, outputs `AVFrame` | `src/media/media_decoder.h` |
| `MediaFilter` | Configures and runs an `AVFilterGraph` pipeline (scale, transpose, overlay, etc.) | `src/media/media_filter.h` |
| `VideoMixer` | Composites multiple video inputs into a single output frame for conferences | `src/media/video/video_mixer.h` |
| `SinkClient` | Exposes decoded video frames to the UI via shared memory (`shm_header.h`) | `src/media/video/sinkclient.h` |
| `SystemCodecContainer` | Probes FFmpeg at startup for available encoders/decoders; provides ranked codec list | `src/media/system_codec_container.h` |
| `CongestionControl` | Implements REMB/TWCC bandwidth estimation; feedback loop to encoders | `src/media/congestion_control.h` |

---

## Media Pipeline Stages

The full data path from local capture to remote rendering consists of the following ordered stages. Both audio and video follow this same logical pipeline, with type-specific class names at each stage.

```
[Capture]
  AudioLayer (PulseAudio/ALSA callback)  VideoInput (V4L2 / screen / file)
       │                                       │
       ▼                                       ▼
[Ring Buffer / Frame Queue]           [VideoFramePassiveReader observer chain]
       │                                       │
       ▼                                       ▼
[Encode]
  AudioSender::update()                VideoSender::encodeAndSendVideo()
    └─ MediaEncoder::encodeAudio()       └─ MediaEncoder::encode(VideoFrame)
         └─ AVCodecContext (libavcodec)        └─ AVCodecContext (libavcodec)
              optional: HardwareAccel               optional: HardwareAccel
       │                                       │
       ▼                                       ▼
[Packetize]
  MediaIOHandle → AVFormatContext mux → AVPacket stream
       │                                       │
       ▼                                       ▼
[RTP Send]
  SocketPair::writeData() → SRTP encrypt (libsrtp) → dhtnet::IceSocket::send()
       │ (network)                             │
       ▼                                       ▼
[RTP Receive]
  SocketPair::readCallback() → SRTP decrypt → rtpDataBuff_
       │                                       │
       ▼                                       ▼
[Decode]
  AudioReceiveThread (ThreadLoop)       VideoReceiveThread::decodeFrame()
    └─ MediaDecoder::decode()             └─ MediaDecoder::decode()
         └─ AVCodecContext (libavcodec)        └─ AVCodecContext (libavcodec)
              optional: HardwareAccel               optional: HardwareAccel
       │                                       │
       ▼                                       ▼
[Render]
  RingBuffer::put() → AudioLayer        SinkClient (shared-memory) → UI
  (PulseAudio/ALSA playback callback)   VideoMixer (conference overlay)
```

**RTCP feedback loop** (parallel, not in hot path):
- `SocketPair::saveRtcpRRPacket()` stores Receiver Reports from peer.
- `AudioRtpSession::processRtcpChecker()` (on `rtcpCheckerThread_`, period 4 s) calls `check_RCTP_Info_RR()` → `adaptQualityAndBitrate()` / `dropProcessing()`.
- `VideoRtpSession` likewise samples `getRtcpRR()` / `getRtcpREMB()`; `CongestionControl::kalmanFilter()` estimates one-way delay gradient; `VideoSender::setBitrate()` feeds back to `MediaEncoder::setBitrate()`.

---

## Critical Code Paths

### Audio Path

```
AudioLayer callback (hw thread)
  → RingBufferPool::putData()
  → AudioInput::update() (ThreadLoop)
  → AudioSender::update()
      → MediaEncoder::encodeAudio(AudioFrame&)
          → avcodec_send_frame() / avcodec_receive_packet()
      → SocketPair::writeData()   [SRTP encrypt inline]
          → dhtnet::IceSocket::send()

AudioReceiveThread::ThreadLoop
  → SocketPair::readRtpData()   [SRTP decrypt inline]
  → AudioDecoder::decode()
      → avcodec_send_packet() / avcodec_receive_frame()
  → RingBuffer::put()
  → AudioLayer playback callback reads RingBuffer
```

Key classes at encode: `AudioSender` → `MediaEncoder`. `MediaEncoder` stores `std::vector<AVCodecContext*> encoders_` and uses `std::mutex encMutex_`.

### Video Path

```
VideoInput::ThreadLoop (V4L2 read or screen grab)
  → VideoFramePassiveReader observer notification
  → VideoSender::update() → encodeAndSendVideo()
      → MediaEncoder::encode(VideoFrame, is_keyframe, frame_number)
          → optional VideoScaler (libswscale)
          → optional HardwareAccel (VAAPI / VideoToolbox / MediaCodec)
          → avcodec_send_frame() / avcodec_receive_packet()
      → SocketPair::writeData()

VideoReceiveThread::ThreadLoop (startLoop/stopLoop)
  → SocketPair::readRtpData()
  → MediaDecoder::decode()
  → VideoGenerator → SinkClient (shm_header.h shared-memory ring)
  → VideoMixer::update() (if in conference)
```

Conference path adds `VideoMixer::ThreadLoop` which composites `N` decoded frames via `VideoScaler` into one output frame, then feeds `VideoSender`.

### SRTP Negotiation

`SocketPair::createSRTP(out_suite, out_params, in_suite, in_params)` initialises a `SRTPProtoContext` (libsrtp2 `srtp_create()`). Cipher suites supported: `AES_CM_128_HMAC_SHA1_80`, `AES_CM_128_HMAC_SHA1_32`. Called from `AudioRtpSession::start()` / `VideoRtpSession::start()` after ICE negotiation completes and keying material has been exchanged over the signalling path (ZRTP or DTLS-SRTP key derivation from SDP `a=crypto:`). If `createSRTP()` throws `std::runtime_error`, the session falls back to plain RTP or aborts.

---

## Threading Model

Every `RtpSession` concrete subclass manages multiple `ThreadLoop` / `InterruptedThreadLoop` instances:

| Thread | Owner class | Lifecycle method | Rate |
|---|---|---|---|
| Sender (audio) | `AudioRtpSession` | `startSender()` / `stop()` | audio frame rate (~50 fps at 20 ms frames) |
| Receiver (audio) | `AudioReceiveThread` | `startReceiver()` / `stopReceiver()` | blocks on `SocketPair::readRtpData()` |
| RTCP checker (audio) | `AudioRtpSession::rtcpCheckerThread_` (`InterruptedThreadLoop`) | `start()` / `stop()` | every 4 s |
| Sender (video) | `VideoRtpSession` | `startSender()` / `stop()` | input FPS (up to 30) |
| Receiver (video) | `VideoReceiveThread` | `startLoop()` / `stopLoop()` | blocks on demuxer |
| Conference mixer | `VideoMixer` | `start()` / `stop()` | 30 fps |
| Audio hardware callback | `AudioLayer` (platform thread) | platform-managed | ~50 Hz |

**Synchronisation rules observed in the code:**
- `AudioSender` and `AudioReceiveThread` share a `SocketPair` protected by `SocketPair::dataBuffMutex_` + `cv_`.
- `MediaEncoder` uses `encMutex_` for the `setBitrate()` / `setPacketLoss()` paths that touch `encoders_[i]` while the sender loop may be encoding concurrently.
- `SocketPair::rtcpInfo_mutex_` guards `listRtcpRRHeader_` and `listRtcpREMBHeader_` between the receiver thread (writers) and the RTCP checker thread (readers).
- `VideoRtpSession::mutex_` (inherited `RtpSession::mutex_`, a `std::recursive_mutex`) protects start/stop/update operations.

---

## Hot Path Warning

> **⚠️ Per-packet instrumentation is PROHIBITED on the send/receive hot path.**

The sender and receiver `ThreadLoop` iterations execute at **20–50 iterations per second for audio** and **up to 30 iterations per second for video**. At these rates:
- Creating an OTel **Span** per packet would generate 50–100 spans/second *per active call*, causing:
  - Prohibitive memory allocation pressure (each span = heap allocation).
  - Contention on the OTel span exporter queue.
  - Measurable CPU overhead degrading the real-time send loop.
- Even attribute lookup on a context-propagation carrier per packet is too expensive.

**Permitted instrumentation on the hot path:** monotonic `Counter::Add()` and `Histogram::Record()` calls, which are lock-free in the OpenTelemetry C++ SDK (backed by atomic operations). These are safe within the sender/receiver `ThreadLoop::process()` body.

**Permitted span creation:** only at session boundary events: `RtpSession::start()`, `RtpSession::stop()`, `MediaEncoder` initialisation (codec negotiation), hardware acceleration fallback events, and quality degradation trigger points (`adaptQualityAndBitrate()`). These are rare, low-frequency operations.

---

## Quality Indicators

The following measurable quality signals are available from existing class APIs, with no source modification required for extraction:

| Signal | Source | Extraction Point | Unit |
|---|---|---|---|
| RTP packet loss (fraction) | `rtcpRRHeader::fraction_lost` | `SocketPair::getRtcpRR()` | 0–255 / 256 |
| RTP cumulative packet loss | `rtcpRRHeader::cum_lost_packet` | `SocketPair::getRtcpRR()` | packets |
| RTP jitter | `rtcpRRHeader::jitter` | `SocketPair::getRtcpRR()` | RTP timestamp units |
| Round-trip latency | `SocketPair::getLastLatency()` | sampled by RTCP checker | ms |
| One-way delay gradient | `CongestionControl::kalmanFilter()` | video RTCP path | ms/packet |
| Bandwidth state | `CongestionControl::get_bw_state()` | video RTCP loop | enum `BandwidthUsage` |
| REMB estimated bitrate | `CongestionControl::parseREMB()` | `VideoRtpSession` RTCP path | bps |
| Video current bitrate | `VideoBitrateInfo::videoBitrateCurrent` | `VideoRtpSession::getVideoBitrateInfo()` | kbps |
| Audio codec name | `MediaEncoder::getAudioCodec()` | post-negotiation | string |
| Video codec name | `MediaEncoder::getVideoCodec()` | post-negotiation | string |
| Encode frame number | `VideoSender::frameNumber_` (private) | needs accessor or metric hook | frames |
| Sequence number wrap | `SocketPair::lastSeqValOut()` | sender loop | uint16 |
| HW accel failures | `MediaDecoder::accelFailures_` (private) | needs accessor or metric hook | count |

RTCP quality data (`RTCPInfo` struct in both `AudioRtpSession` and `VideoRtpSession`) aggregates `packetLoss`, `jitter`, `nb_sample`, and `latency` — already computed by `check_RCTP_Info_RR()` every 4 seconds, making it the natural sampling point for metrics emission.

---

## External Dependencies

| Library | Role | Key APIs used |
|---|---|---|
| **libavcodec** (FFmpeg) | Audio/video encode and decode | `avcodec_send_frame()`, `avcodec_receive_packet()`, `avcodec_send_packet()`, `avcodec_receive_frame()`, `AVCodecContext` |
| **libavformat** (FFmpeg) | RTP mux/demux (packetize/depacketize) | `AVFormatContext`, `av_interleaved_write_frame()`, `avformat_open_input()` |
| **libavfilter** (FFmpeg) | `MediaFilter` graph (scale, transpose, overlay) | `avfilter_graph_alloc()`, `av_buffersrc_add_frame()` |
| **libswresample** (FFmpeg) | `Resampler` — sample-rate and channel-format conversion | `swr_convert()` |
| **libswscale** (FFmpeg) | `VideoScaler` — pixel-format and resolution scaling | `sws_scale()` |
| **libsrtp2** | SRTP encrypt/decrypt in `SocketPair` via `SRTPProtoContext` | `srtp_create()`, `srtp_protect()`, `srtp_unprotect()` |
| **Speex DSP** | `SpeexEchoState` acoustic echo cancellation in audio-processing/ | `speex_echo_cancellation()` |
| **webrtc-audio-processing** | Noise suppression, AGC, VAD in `AudioProcessor` (`src/media/audio/audio-processing/`) | WebRTC APM C++ API |
| **PulseAudio / ALSA / CoreAudio / AAudio / JACK / PortAudio** | Platform audio HAL (`AudioLayer` subclasses) | Platform-specific |
| **V4L2** | Linux video capture in `src/media/video/v4l2/` | `ioctl(VIDIOC_DQBUF)` |
| **dhtnet** | ICE transport socket (`dhtnet::IceSocket`) passed to `RtpSession::start()` | `IceSocket::send()`, `IceSocket::recv()` |
| **asio** | `asio::steady_timer` for RTCP timeout and decoder stream-info probe | `async_wait()` |
| **VAAPI / VideoToolbox / MediaCodec** | Hardware accelerated encode/decode (`HardwareAccel` in `src/media/video/accel.h`) | Platform-specific buffer APIs |

---

## Estimated Instrumentation Value

**Very High.** The media pipeline runs continuously during active calls and is the primary source of user-visible quality problems: packet loss, jitter, codec negotiation failure, AEC/noise-suppression failure, hardware acceleration fallback. Metric instrumentation of RTCP-derived signals (already computed every 4 s), bitrate adaptations, and codec selection events would provide direct call-quality observability with negligible runtime overhead if implemented correctly on the metrics-only policy described in the Hot Path Warning section above.

---

## Open Questions

1. Is congestion control (REMB/TWCC) enabled by default for all calls, or only for JAMI P2P calls?
2. How is the `VideoMixer` layout algorithm determined — is it fixed grid or negotiated with participants?
3. What is the shared-memory mechanism used by `SinkClient` — is it POSIX shm or a custom approach?
4. Are `MediaFilter` graphs constructed per-call or shared across calls?
5. What happens to the encode thread when the ICE socket drops mid-call — is there a reconnect path?
