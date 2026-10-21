# ADR-003: Swappable In-Daemon Audio Processing, WebRTC by Default

**Status:** Accepted
**Origin:** Retrospective
**Decision date:** 2021-02
**Recorded:** 2026-10-21

## Context

Calls need echo cancellation, noise suppression and gain control on every
platform. Some platforms provide these natively, others do not, and the
existing speexdsp echo canceller did not work.

## Decision

Audio processing runs in `AudioLayer` behind the `AudioProcessor`
interface (`src/media/audio/audio-processing/`), with `webrtc`, `speex`
and `null` implementations selected by preference. `webrtc`
(webrtc-audio-processing, a contrib) is the default. With echo
cancellation or noise suppression set to `auto`, the daemon's processor
is used only when the platform layer reports no native one; on Linux,
echo cancellation defaults to the daemon's processor.

## Consequences

- Audio layers must report native AEC/NS so audio is not processed twice.
- webrtc-audio-processing is a foundational contrib on all platforms.
- New processing features (VAD, AGC) extend the interface, not individual
  audio layers.

## Historical evidence

- **Facts:** `e787de4ea` (2021-02) introduces the `EchoCanceller`
  abstraction with a null default. `854362a8c` (2021-02) adds the
  webrtc-audio-processing implementation and disables system AEC on
  Windows. `5d0247d27` (2021-03) adds the contrib. `aa9a39e26` (2022-07)
  renames it `AudioProcessor` and adds preferences. Defaults are in
  `src/preferences.cpp`.
- **Documented rationale:** `e787de4ea`: implementations "may be platform
  specific compile-time or swapped out at runtime"; "the speexdsp aec is
  not currently functional".
- **Reconstructed:** why webrtc-audio-processing was chosen over other
  libraries is not recorded.
