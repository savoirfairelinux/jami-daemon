# WebRTC Interop Tools

These tools validate interoperability between Jami and external WebRTC implementations built with `werift` and a real browser `RTCPeerConnection` implementation.

Current scope:
- Jami offer -> werift answer
- werift offer -> Jami answer
- BUNDLE, MID, `rtcp-mux`, and `UDP/TLS/RTP/SAVPF` negotiation
- Live ICE connectivity and DTLS-SRTP handshake in both directions
- A minimal encrypted RTP packet from Jami to werift
- A minimal encrypted RTCP receiver report from werift back to Jami
- A minimal encrypted RTP packet sent by werift after the live session is established, reported as a return-path diagnostic
- Browser SDP parsing/generation and live ICE/DTLS connectivity through Chrome or Chromium
- Browser codec negotiation constrained through `RTCRtpTransceiver.setCodecPreferences()` to the common Opus/PCMU/PCMA/G722 and VP8 subset

Current non-goals:
- Full audio or video playout validation
- Long-running media quality checks
- SCTP or data channel interoperability

The bridge still supports the original SDP-only modes, but now also exposes live modes that create a real ICE transport, negotiate DTLS-SRTP, emit one encrypted RTP packet, and assert encrypted RTCP return traffic.

## Prerequisites

- A built `jami_webrtc_sdp_bridge` binary
- Node.js with npm
- Chrome or Chromium for browser interop checks

## Build

CMake:

```sh
cmake --build build --target jami_webrtc_sdp_bridge
```

Meson:

```sh
ninja -C build-meson jami_webrtc_sdp_bridge
```

## Install Node dependency

```sh
cd test/webrtc_interop
npm install
```

## Run

If the bridge was built in the default CMake directory, the script auto-detects it and runs both the SDP-only checks and the live ICE/DTLS checks:

```sh
cd test/webrtc_interop
node interop.js
```

Run the browser interop checks with Chrome or Chromium:

```sh
cd test/webrtc_interop
npm run interop:browser
```

If Chrome lives outside the default macOS application paths, set `JAMI_WEBRTC_BROWSER` or `CHROME_BIN` to the browser binary.

If the bridge lives elsewhere, pass its path explicitly:

```sh
cd test/webrtc_interop
node interop.js ../../build-meson/jami_webrtc_sdp_bridge
```

The browser script accepts the same explicit bridge path:

```sh
cd test/webrtc_interop
node browser_interop.js ../../build-meson/jami_webrtc_sdp_bridge
```