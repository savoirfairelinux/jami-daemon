# QoS Test Lab — Audio Priority Testing

## Overview

This test infrastructure measures Jami's behavior under degraded network
conditions, specifically to evaluate and compare QoS strategies that
prioritize audio over video.

## Two-Level Approach

### Level 1: In-Process Simulator (`NetworkSimulator`)

A C++ class (`daemon/src/media/network_sim/`) that intercepts packets at the
`SocketPair` layer. Allows writing unit tests that configure packet loss,
bandwidth limits, and jitter without any network setup.

**Source files:**
- `daemon/src/media/network_sim/network_simulator.h` — Simulator with token bucket + random loss
- `daemon/src/media/network_sim/network_simulator.cpp` — Implementation
- `daemon/src/media/network_sim/network_sim_registry.h` — Global registry to attach simulators to calls

**Integration point:**
- `daemon/src/media/socket_pair.h` — `setNetworkSimulator()` method
- `daemon/src/media/socket_pair.cpp` — `writeCallback()` checks simulator before sending

**Usage in tests:**
```cpp
#include "media/network_sim/network_sim_registry.h"
#include "media/socket_pair.h"

// Create simulators via the registry
auto& reg = NetworkSimRegistry::instance();
auto videoSim = reg.getOrCreate(callId, "video");
auto audioSim = reg.getOrCreate(callId, "audio");

// Wire simulators into the socket pairs (must be done explicitly by the test)
videoSocketPair->setNetworkSimulator(videoSim);
audioSocketPair->setNetworkSimulator(audioSim);

// Simulate a constrained link (300Kbps shared)
videoSim->setEnabled(true);
videoSim->setBandwidthLimit(250'000); // 250 Kbps for video
videoSim->setPacketLoss(0.05f);       // 5% loss on video

audioSim->setEnabled(true);
audioSim->setBandwidthLimit(50'000);  // 50 Kbps for audio (plenty for Opus)
audioSim->setPacketLoss(0.01f);       // 1% loss on audio

// ... run call for N seconds ...

auto videoStats = videoSim->getStats();
auto audioStats = audioSim->getStats();

// Verify audio gets preferential treatment
float audioLoss = audioSim->getObservedPacketLoss();
float videoLoss = videoSim->getObservedPacketLoss();
CPPUNIT_ASSERT(audioLoss < 0.05f); // Audio must maintain < 5% loss
```

### Level 2: Network Lab with tc/netem

Shell scripts that create Linux network namespaces and apply real traffic
shaping using `tc`/`netem`. Two Jami instances run in separate namespaces
with configurable degradation between them.

**Scripts:**
- `setup-qos-lab.sh` — Create/destroy the virtual network, apply profiles
- `run-qos-scenario.sh` — Automated measurement across multiple profiles

**Quick start:**
```bash
cd daemon/test/virtual-networking/qos-test/

# Create the virtual network
sudo ./setup-qos-lab.sh up

# Open shells in each namespace
sudo ip netns exec peer-a bash   # Terminal 1
sudo ip netns exec peer-b bash   # Terminal 2

# Start Jami in each (with logging)
# Terminal 1: jami -d > /tmp/jami-peer-a.log 2>&1
# Terminal 2: jami -d > /tmp/jami-peer-b.log 2>&1

# Apply degradation (from any terminal with root)
sudo ./setup-qos-lab.sh profile poor      # 100ms delay, 5% loss, 300Kbit
sudo ./setup-qos-lab.sh profile terrible  # 200ms delay, 15% loss, 150Kbit
sudo ./setup-qos-lab.sh profile good      # Clear all impairment

# Custom netem parameters
sudo ./setup-qos-lab.sh custom "delay 80ms 20ms loss 10% rate 200kbit"

# Tear down
sudo ./setup-qos-lab.sh down
```

**Available profiles:**

| Profile    | Delay       | Loss | Bandwidth |
|-----------|-------------|------|-----------|
| good       | —          | —    | —         |
| moderate   | 50ms ±10   | 1%   | 1 Mbit   |
| poor       | 100ms ±30  | 5%   | 300 Kbit |
| terrible   | 200ms ±80  | 15%  | 150 Kbit |
| audio-only | 50ms ±10   | 2%   | 50 Kbit  |

## Testing QoS Strategies

To compare strategies, run the same call scenario under the same profile with
different QoS configurations:

1. **Baseline** — No QoS (current behavior)
2. **DSCP marking** — Audio marked EF, video marked AF41
3. **Aggressive video bitrate reduction** — Video drops to minimum when audio loss detected
4. **Combined** — DSCP + bitrate adaptation

For each run, measure:
- Audio packet delivery ratio (from RTCP RR)
- Video packet delivery ratio
- Audio MOS (if measurable) or jitter
- Video bitrate achieved
- Time-to-recovery after degradation

## Prerequisites

- Linux kernel with `sch_netem` module (standard on most distros)
- `iproute2` package (provides `tc` and `ip`)
- Root access (for network namespaces and tc)
- Jami daemon built with debug logging (`--debug`)
