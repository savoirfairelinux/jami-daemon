# DHT Layer

## Status: draft

## Last Updated: 2026-03-13

---

## Purpose

The DHT layer is the backbone of JAMI's decentralized identity and communication model. It integrates the OpenDHT distributed hash table library to provide: node lifecycle management (bootstrap, routing table maintenance, announce), peer discovery by Ed25519 public key hash, encrypted value put/get for call invitation and device synchronization, name resolution via a pluggable name directory server, and the `dhtnet` connection manager that multiplexes authenticated TLS channels over the DHT for data and signaling transport.

---

## Key Files

- `src/jamidht/jamiaccount.h` / `.cpp` — `JamiAccount`: owns `dht::DhtRunner`, drives all DHT operations
- `src/jamidht/account_manager.h` / `.cpp` — `AccountManager`: identity keypair, contact trust management
- `src/jamidht/archive_account_manager.h` / `.cpp` — encrypted local archive of DHT identity
- `src/jamidht/server_account_manager.h` / `.cpp` — JAMS (server-hosted) identity variant
- `src/jamidht/namedirectory.h` / `.cpp` — `NameDirectory`: HTTP lookup of username↔DHT address
- `src/jamidht/contact_list.h` / `.cpp` — `ContactList`: trusted/blocked peer record
- `src/jamidht/jami_contact.h` — `Contact` struct, trust status
- `src/jamidht/configkeys.h` — DHT-specific config key constants
- `src/jamidht/channeled_transport.h` / `.cpp` — SIP-over-dhtnet channel bridge
- `src/jamidht/auth_channel_handler.h` / `.cpp` — authentication handshake over dhtnet channel
- `src/gittransport.h` / `.cpp` — libgit2 smart transport over dhtnet for conversation sync
- `src/jamidht/eth/` — Ethereum-style address derivation utilities

---

## Key Classes

| Class | Role | File |
|---|---|---|
| `JamiAccount` | Owns `dht::DhtRunner` (exposed as `dht_`); calls `dht_->run()`, `->bootstrap()`, `->listen<T>()`, `->cancelListen()`; manages ICE/SIP channel lifecycle | `src/jamidht/jamiaccount.h` |
| `AccountManager` | Holds `AccountInfo` (Ed25519 identity + contacts); `forEachDevice()` does a DHT `get` to enumerate peer devices; `findCertificate()` does a DHT `get` by `InfoHash`/`PkId`; `startSync()` triggers `identityAnnouncedCb` | `src/jamidht/account_manager.h` |
| `ArchiveAccountManager` | Encrypts/decrypts an account archive for local identity backup | `src/jamidht/archive_account_manager.h` |
| `ServerAccountManager` | Uses JAMS REST API as identity backend instead of local archive | `src/jamidht/server_account_manager.h` |
| `ContactList` | Persists trusted/blocked state, pending trust requests; triggers callbacks (`OnContactAdded`, `OnDevicesChanged`) on change | `src/jamidht/contact_list.h` |
| `NameDirectory` | HTTP client for `ns.jami.net` (default); `lookupAddress()`, `lookupName()`, `registerName()` via `dht::http::Request` | `src/jamidht/namedirectory.h` |
| `ChanneledTransport` | Bridges `dhtnet::ChannelSocket` as a PJSIP transport so SIP signaling runs over DHT channels | `src/jamidht/channeled_transport.h` |
| `AuthChannelHandler` | Executes the mutual-authentication handshake when a new dhtnet channel is opened | `src/jamidht/auth_channel_handler.h` |

---

## DHT Operations

The following `dht::DhtRunner` operations are used directly in `JamiAccount`:

| Operation | Call site in `jamiaccount.cpp` | Purpose |
|---|---|---|
| `dht_->run(port, config, context)` | `JamiAccount::doRegister_()` ~line 1970 | Start DHT node; binds UDP port in `DHT_PORT_RANGE {4000, 8888}` |
| `dht_->bootstrap(host)` | `JamiAccount::doRegister_()` ~line 1983 | Seed routing table from `loadBootstrap()` list |
| `dht_->listen<DeviceAnnouncement>(h, cb)` | `JamiAccount::trackPresence()` ~line 1789 | Subscribe to device presence announcements for tracked peers; returns `listenToken` stored in `BuddyInfo` |
| `dht_->cancelListen(h, listenToken)` | `JamiAccount::trackBuddyPresence()` ~line 1776 | Unsubscribe from presence updates |
| `dht_->connectivityChanged()` | UPnP mapping callback in `doRegister_()` ~line 1972 | Notify DHT of address change after UPnP port allocation |
| `dht_->join()` | `JamiAccount::doRegister_()` guard | Stop running DHT node before restart |
| `dht_->isRunning()` | `JamiAccount::trackPresence()` guard | Guard for running state |
| `dht_->get<T>(h, cb)` | Via `AccountManager::forEachDevice()`, `findCertificate()` | Look up device public keys and certificates by `InfoHash` |
| `dht_->put(h, value)` | Via `AccountManager::startSync()` / identity announce flow | Publish device announcement / presence value |

The `identityAnnouncedCb` lambda in `JamiAccount::initDhtContext()` (~line 2127) is invoked by OpenDHT when the node's identity is successfully published to the network. It triggers `AccountManager::startSync()` which begins the device-discovery `get`/`listen` loop and calls `onAccountDeviceAnnounced()`, which then bootstraps all active conversations via `ConversationModule::bootstrap()`.

---

## Critical Code Paths

### 1. Account Registration / DHT Bootstrap

```
JamiAccount::doRegister()
  → JamiAccount::doRegister_()
      → dht_->run(port, config, context)          // start node
      → dht_->bootstrap(host)                     // connect to network
      → accountManager_->setDht(dht_)
      → initConnectionManager()                   // create dhtnet::ConnectionManager
      → connectionManager_->dhtStarted()
      → [identityAnnouncedCb fires from OpenDHT thread]
          → accountManager_->startSync(...)
              → [OpenDHT: dht_->put(identity announce)]
              → onAccountDeviceAnnounced()
                  → convModule()->bootstrap()
```

### 2. Peer Discovery for Outgoing Call

```
JamiAccount::newOutgoingCall(toUrl, mediaList)
  → call->createIceMediaTransport() + initIceMediaTransport()   // via connectionManager_->getIceOptions()
  → JamiAccount::newOutgoingCallHelper(call, uri)
      → JamiAccount::startOutgoingCall(call, toUri)
          → accountManager_->lookupAddress(toUri, ...)           // NameDirectory HTTP lookup (non-blocking)
          → [iterate cached sipConns_ for existing channel]
          → accountManager_->forEachDevice(peer_account, ...)    // DHT get — enumerate peer devices
              [per device found:]
                  → requestSIPConnection(toUri, deviceId, "videoCall"|"audioCall", true, dev_call)
                      → connectionManager_->connectDevice(...)   // dhtnet ICE + TLS channel open
```

### 3. Username → DHT Key Resolution (NameDirectory)

```
JamiAccount::newOutgoingCallHelper()  [when startOutgoingCall() throws]
  → NameDirectory::lookupUri(suffix, nameServer, cb)
      → NameDirectory::instance(ns).lookupName(name, cb)
          → dht::http::Request GET https://ns.jami.net/name/<name>
          → cb(regName, address, Response::found|notFound)
      → runOnMainThread(...)
          → JamiAccount::startOutgoingCall(call, regName)
```

Also on account registration:
```
JamiAccount::doRegister_()
  → accountManager_->lookupAddress(accountId, ...)
      → NameDirectory::lookupAddress(addr, cb)
          → dht::http::Request GET https://ns.jami.net/addr/<addr>
      → JamiAccount::lookupRegisteredName(regName, response)
```

### 4. Buddy Presence Tracking

```
JamiAccount::trackBuddyPresence(buddy_id, track=true)
  → JamiAccount::trackPresence(h, buddy)
      → dht_->listen<DeviceAnnouncement>(h, cb)   // persistent DHT subscription
          [callback on OpenDHT thread when device announces]:
              → buddy.devices_cnt++/--
              → runOnMainThread(...)
                  → onTrackedBuddyOnline(h) / onTrackedBuddyOffline(h)
                      → emitSignal<PresenceSignal::NewBuddyNotification>
```

### 5. Incoming Connection / ICE Negotiation

```
[Peer initiates dhtnet connection]
  → dhtnet::ConnectionManager::onICERequest callback
      → JamiAccount::onICERequest(deviceId)
          → accountManager_->findCertificate(deviceId, cb)   // DHT cert lookup
          → trust check → return accept/reject
  → [ICE negotiation completes in dhtnet]
  → connectionManager_->onConnectionReady callback
      → JamiAccount::onConnectionReady(deviceId, name, channel)
          [if name == "audioCall"|"videoCall"]
              → newIncomingCall(from, mediaList, transport)
```

---

## Threading Model

- **OpenDHT internal thread pool** (`dht::ThreadPool`): `DhtRunner` maintains its own executor; all DHT network I/O and callback delivery happen on these threads. The `identityAnnouncedCb`, value-found callbacks from `listen<>`, and `get<>` result callbacks all arrive on OpenDHT threads.
- **`dht::ThreadPool::io().run()`**: Used explicitly in `JamiAccount` (e.g., `onAccountDeviceFound()`, `onConnectedOutgoingCall()`) to run continuations on the shared OpenDHT I/O thread pool rather than holding up a DHT callback.
- **`runOnMainThread()`**: Used inside DHT callbacks (e.g., in `trackPresence()` callback, ~line 1806) to marshal state changes back to the JAMI main thread where `configurationMutex_` and other account-level locks are expected.
- **ASIO io_context** (shared daemon-wide): `NameDirectory` HTTP requests (`dht::http::Request`) are executed on the ASIO thread; completions arrive there too. `asio::steady_timer` instances (e.g., inside `DiscoveredPeer`) use the same context.
- **`connManagerMtx_` (shared_mutex)**: Guards `connectionManager_` initialization and access; `initConnectionManager()` acquires unique lock; `requestSIPConnection` and call-path code acquires shared lock.
- **`buddyInfoMtx` (mutex)**: Protects `trackedBuddies_` map accessed from both the main thread and OpenDHT callback threads.
- **`sipConnsMtx_` (mutex)**: Protects `sipConns_` cache of open SIP channels accessed in both call initiation and connection-ready callbacks.
- **No ASIO strands for DHT callbacks**: DHT result callbacks are not wrapped in ASIO strands; thread safety relies on `runOnMainThread()` marshalling or fine-grained per-data-structure mutexes.

---

## Key Operations with Expected Latency

| Operation | Typical Latency | Worst Case | Notes |
|---|---|---|---|
| DHT bootstrap (first connection) | 500ms – 3s | 10s+ | Depends on network reachability and bootstrap node responsiveness |
| DHT `listen<DeviceAnnouncement>` round-trip | 200ms – 2s | 10s | Subscription confirmed after first `get` sweep of routing table |
| `AccountManager::forEachDevice()` (DHT `get`) | 100ms – 3s | 10s | Queries up to K=20 closest nodes; latency dominated by routing table convergence after bootstrap |
| `AccountManager::findCertificate()` DHT `get` | 100ms – 2s | 5s | Certificate may be cached locally (`certStore()`); DHT lookup only if missed |
| `NameDirectory::lookupName()` / `lookupAddress()` HTTP | 100ms – 1s | 5s | Single HTTPS GET to `ns.jami.net`; failure falls back gracefully |
| `NameDirectory::registerName()` HTTP | 300ms – 2s | 10s | POST + signature verification on server |
| ICE candidate gathering (`connectionManager_->getIceOptions()`) | 50ms – 500ms | 2s | STUN binding requests; UPnP mapping may add 1-3s on first run |
| dhtnet channel open (ICE + TLS handshake) | 500ms – 5s | 30s | Includes DHT signaling round-trip, ICE negotiation, TLS handshake |
| SIP INVITE delivery over dhtnet channel | 10ms – 100ms | 1s | Once channel is open; pure in-memory transport latency |
| Identity announcement (`dht_->put`) | 200ms – 3s | 10s | Blocked on DHT node having sufficient routing state |

---

## External Dependencies

- **OpenDHT** (`opendht/dhtrunner.h`, `opendht/default_types.h`, `opendht/crypto.h`, `opendht/peer_discovery.h`, `opendht/thread_pool.h`, `opendht/http.h`) — DHT node, Ed25519/curve25519 crypto, value types, local LAN peer discovery, HTTP client
- **dhtnet** (`dhtnet/connectionmanager.h`, `dhtnet/multiplexed_socket.h`, `dhtnet/certstore.h`, `dhtnet/tls_session.h`, `dhtnet/diffie-hellman.h`, `dhtnet/ice_transport.h`, `dhtnet/ice_transport_factory.h`, `dhtnet/upnp/upnp_control.h`) — multiplexed TLS channels over DHT, ICE negotiation, UPnP port mapping, certificate trust store
- **libgit2** (via `src/gittransport.h`) — git smart protocol over dhtnet for conversation repository sync
- **jsoncpp** — DHT value payload encoding (MIME type `application/invite+json`)
- **msgpack** — contact list and device sync data serialization
- **asio** — timer-driven bootstrap retries, async HTTP, `DiscoveredPeer` cleanup timers
- **PJSIP** — SIP stack; `ChanneledTransport` wraps `dhtnet::ChannelSocket` as a PJSIP transport

---

## Coupling Map

```
JamiAccount (DHT layer)
├── → dht::DhtRunner              [direct ownership; all DHT put/get/listen]
├── → dhtnet::ConnectionManager   [channel open/close; ICE; protected by connManagerMtx_]
│       ├── onICERequest()        → AccountManager::findCertificate()   [cert trust check]
│       ├── onChannelRequest()    → JamiAccount::onChannelRequest()     [name-based dispatch]
│       └── onConnectionReady()   → JamiAccount::onConnectionReady()    [SIP transport creation]
├── → AccountManager              [identity, forEachDevice, findCertificate, startSync]
│       └── → NameDirectory       [HTTP name resolution; shared singleton per nameServer URL]
│       └── → ContactList         [trust state; device known-device map]
├── → ConversationModule          [bootstrap() called after identity announce; git sync via dhtnet]
├── → SyncModule                  [device-to-device state sync over dhtnet "sync" channel]
├── → Manager (call_manager)      [callFactory.newSipCall(); newIncomingCall() signal dispatch]
├── → SIPVoIPLink                 [PJSIP integration; SIPStartCall via ChanneledTransport]
└── → MessageEngine               [onPeerOnline() called from trackPresence/presence callback]
```

Key invariants:
- `dht_` is `std::shared_ptr<dht::DhtRunner>` set in `JamiAccount` constructor; `AccountManager` gets a copy via `setDht()`.
- `connectionManager_` is initialized lazily in `initConnectionManager()` (called from `doRegister_()` and `onAccountDeviceFound()`). It is null before registration and after `shutdownConnections()`.
- `NameDirectory` is a singleton per server URL (created by `AccountManager` constructor via `NameDirectory::instance(nameServer)`).

---

## Estimated Instrumentation Value

**High.** DHT bootstrap latency, `forEachDevice` duration, and dhtnet channel open latency are the dominant contributors to call setup time in JAMI. Tracing these operations end-to-end would directly explain perceived call setup delay. ICE failure and DHT lookup failure rates are critical for diagnosing connectivity in restricted environments (corporate firewalls, NAT double layers).

---

## Open Questions

1. What is the DHT port range strategy when ports 4000–8888 are fully blocked? Does the proxy fallback (`config.proxy_server`) cover this automatically?
2. Does `JamiAccount` have a minimum DHT peer count gate before allowing calls — or is the first `forEachDevice` attempt made as soon as `identityAnnouncedCb` fires?
3. Is `ChanneledTransport` (SIP over DHT channel) always used for JAMI calls, or is direct UDP/ICE used instead when possible? Evidence suggests dhtnet channel is always the transport.
4. What is the relationship between `ethAccount` in `AccountInfo` and the DHT identity — is Ethereum used for anything at runtime, or is it legacy?
5. Is `dht::PeerDiscovery` (LAN peer discovery, `startAccountDiscovery()`) instrumented separately from the wider DHT; should it get its own span/metric?
