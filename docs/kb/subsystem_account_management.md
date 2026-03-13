# Account Management

## Status: draft

## Last Updated: 2026-03-13

---

## Purpose

The account management subsystem handles the full lifecycle of communication accounts within libjami. It provides a polymorphic account hierarchy supporting two distinct protocol families — vanilla SIP (RFC 3261, managed via PJSIP) and JAMI/DHT accounts (P2P, managed via OpenDHT + dhtnet). Responsibilities include account creation and deletion, credential storage, registration state machines, contact list and trust management, multi-device linking, account archival/backup/restore, and JAMI-specific features such as name directory lookups and device announcement.

---

## Key Files

- `src/account.h` / `src/account.cpp` — abstract base class `Account`
- `src/account_config.h` / `src/account_config.cpp` — base `AccountConfig` (serializable)
- `src/account_factory.h` / `src/account_factory.cpp` — `AccountFactory` (creates/destroys accounts by type string)
- `src/account_schema.h` — constant key names for account details maps
- `src/registration_states.h` — `RegistrationState` enum
- `src/sip/sipaccountbase.h` / `.cpp` — `SIPAccountBase` (common SIP logic)
- `src/sip/sipaccountbase_config.h` / `.cpp` — `SipAccountBaseConfig`
- `src/sip/sipaccount.h` / `src/sip/sipaccount.cpp` — `SIPAccount` (SIP registrar, presence)
- `src/sip/sipaccount_config.h` / `.cpp` — `SipAccountConfig`
- `src/jamidht/jamiaccount.h` / `src/jamidht/jamiaccount.cpp` — `JamiAccount` (DHT, swarm, conversations)
- `src/jamidht/jamiaccount_config.h` / `.cpp` — `JamiAccountConfig`
- `src/jamidht/account_manager.h` / `.cpp` — `AccountManager` (identity, contacts, trust)
- `src/jamidht/archive_account_manager.h` / `.cpp` — local encrypted archive
- `src/jamidht/server_account_manager.h` / `.cpp` — JAMS (Jami Account Management Server) backend
- `src/jamidht/contact_list.h` / `.cpp` — `ContactList`
- `src/jamidht/jami_contact.h` — `Contact` / `TrustStatus` structs
- `src/jamidht/namedirectory.h` / `.cpp` — `NameDirectory` (HTTP-based username↔address resolution)
- `src/jamidht/accountarchive.h` / `.cpp` — `AccountArchive` (encrypted backup)

---

## Key Classes

| Class | Role | File |
|---|---|---|
| `Account` | Abstract base; holds accountId, config, call set, ring buffers, message engine | `src/account.h` |
| `AccountConfig` | Serializable config base; serialize/unserialize via YAML | `src/account_config.h` |
| `AccountFactory` | Creates `SIPAccount` or `JamiAccount` by type string; maintains account map | `src/account_factory.h` |
| `SIPAccountBase` | Shared SIP logic: codec negotiation, SRTP, presence base, ICE params | `src/sip/sipaccountbase.h` |
| `SIPAccount` | Full SIP: PJSIP registrar client (`pjsip_regc`), NAT traversal, STUN, TLS transport | `src/sip/sipaccount.h` |
| `JamiAccount` | JAMI P2P: OpenDHT runner, dhtnet `ConnectionManager`, conversation module, sync module | `src/jamidht/jamiaccount.h` |
| `AccountManager` | Identity (crypto keypair), contact trust decisions, device certificate chain | `src/jamidht/account_manager.h` |
| `ArchiveAccountManager` | Reads/writes encrypted `.gz` backup of identity + contacts | `src/jamidht/archive_account_manager.h` |
| `ServerAccountManager` | JAMS REST API authentication instead of local archive | `src/jamidht/server_account_manager.h` |
| `ContactList` | Maintains trusted/blocked/pending contact state; persisted via msgpack | `src/jamidht/contact_list.h` |
| `NameDirectory` | HTTP client for `name.jami.net` username↔DHTpubkey lookup/registration | `src/jamidht/namedirectory.h` |
| `AccountInfo` | Plain struct: identity, deviceId, contacts ptr, ethAccount, username | `src/jamidht/account_manager.h` |

---

## External Dependencies

- **PJSIP / PJNATH** — SIP UA stack used by `SIPAccount` for registrar, dialog, invite
- **OpenDHT** (`opendht/dhtrunner.h`, `opendht/crypto.h`) — DHT node and cryptographic identity
- **dhtnet** (`dhtnet/connectionmanager.h`, `dhtnet/certstore.h`, `dhtnet/upnp/`) — multiplexed channel sockets, cert store, UPnP
- **yaml-cpp** — account config serialization
- **msgpack** — contact list persistence
- **jsoncpp** — used by `JamiAccount` for DHT value payloads
- **GnuTLS / OpenSSL** (via dhtnet) — certificate generation and TLS

---

## Threading Model

- `Manager::init()` runs on the **main thread**; account loading dispatches work via the global **ASIO `io_context`** thread pool.
- `JamiAccount` operations (DHT lookups, connection management) are fully **asio-async**; callbacks are posted back to the io_context.
- `SIPAccount` registration and re-registration are driven by the **PJSIP event loop** thread (`SIPVoIPLink::sipThread_`).
- Config reads/writes are protected by `configurationMutex_` (per-account `std::mutex`).
- `ContactList` and `AccountManager` callbacks use `OnChangeCallback` lambdas posted to asio.

---

## Estimated Instrumentation Value

**High.** Account registration state changes, device-link events, and trust decisions are infrequent but critical. Tracing registration state transitions (`RegistrationState` enum changes), auth failures, and device announcement timing would substantially aid debugging connectivity issues.

---

## Account Types: SIPAccount vs JamiAccount Differences

| Dimension | `SIPAccount` | `JamiAccount` |
|---|---|---|
| **Protocol** | RFC 3261 SIP via PJSIP | P2P DHT + dhtnet multiplexed sockets |
| **Registration target** | SIP registrar server (`pjsip_regc`) | OpenDHT network (no central server); optional JAMS for managed deployments |
| **Identity model** | Username + password + realm (`pjsip_cred_info`); TLS client cert optional | Cryptographic keypair (`dht::crypto::Identity` = private key + device certificate chain); no shared secret |
| **TLS** | `pjsip_tpfactory` via `TlsListener`; cert loaded from configured file path | `dhtnet::tls::TrustStore` + `dhtnet::ConnectionManager`; device cert generated or restored from archive |
| **Config class** | `SipAccountConfig` | `JamiAccountConfig` |
| **Account manager** | None (credentials in config directly) | `ArchiveAccountManager` (local encrypted `.gz`) or `ServerAccountManager` (JAMS REST) |
| **Transport broker** | `SipTransportBroker` (UDP/TCP/TLS) | `dhtnet::ConnectionManager` (ICE + TLS, multiplexed) |
| **Presence** | SIP SUBSCRIBE/NOTIFY via `SIPPresence` | DHT device-announcement values |
| **Conversation/sync** | Not present | `ConversationModule`, `SyncModule`, git-based conversation history |
| **`doRegister()` entry** | `SIPAccount::doRegister()` → `sendRegister()` → `pjsip_regc_send()` | `JamiAccount::doRegister()` → `doRegister_()` → `AccountManager::initAuthentication()` → `dht_->run()` + `dht_->bootstrap()` |
| **`doUnregister()` entry** | `SIPAccount::sendUnregister()` → `pjsip_regc_unregister()` | `JamiAccount::doUnregister()` → `dht_->join()` teardown + `connectionManager_` shutdown |
| **Registration callback thread** | PJSIP event loop thread (`sipThread_`) | ASIO `io_context` thread pool |
| **Inheritance path** | `Account` → `SIPAccountBase` → `SIPAccount` | `Account` → `SIPAccountBase` → `JamiAccount` (shares SIP transport for call media) |
| **IP2IP** | `SIPAccount::isIP2IP()` returns true for the built-in IP2IP account | N/A |

---

## Account Lifecycle States

### `RegistrationState` Enum (`src/registration_states.h`)

| State | Value | Meaning |
|---|---|---|
| `UNLOADED` | 0 | Account object created but `doRegister()` not yet called |
| `UNREGISTERED` | 1 | Successfully unregistered (clean stop) |
| `TRYING` | 2 | Registration in progress |
| `REGISTERED` | 3 | Successfully registered and operational |
| `ERROR_GENERIC` | 4 | Unclassified registration failure |
| `ERROR_AUTH` | 5 | Authentication refused (wrong credentials / cert rejected) |
| `ERROR_NETWORK` | 6 | Network unreachable |
| `ERROR_HOST` | 7 | Registrar/bootstrap host unreachable |
| `ERROR_SERVICE_UNAVAILABLE` | 8 | Server responded 503 or equivalent |
| `ERROR_NEED_MIGRATION` | 9 | Account archive requires migration before use |
| `INITIALIZING` | 10 | Identity bootstrapping in progress (JamiAccount only) |

### State Transition Diagram

```
               ┌──────────────────────────────────────────────┐
               │                                              ▼
UNLOADED ──doRegister()──→ INITIALIZING ──auth ok──→ TRYING ──ok──→ REGISTERED
                                │                      │               │
                                │              network/host err        │ doUnregister()
                                ▼                      ▼               ▼
                          ERROR_AUTH          ERROR_NETWORK      UNREGISTERED
                          ERROR_NEED_MIGRATION ERROR_HOST             │
                                                ERROR_GENERIC    doRegister()
                                                                      │
                                                               TRYING ──→ REGISTERED
```

- `INITIALIZING` is emitted by `JamiAccount` during `initAuthentication()` before the DHT runner starts.
- `SIPAccount` transitions directly: `UNLOADED → TRYING → REGISTERED/ERROR_*`.
- `setRegistrationState()` emits `libjami::ConfigurationSignal::RegistrationStateChanged` to clients.
- Re-registration refresh (SIP REGISTER with Expires > 0) does **not** re-enter `TRYING`; the state remains `REGISTERED`.
- Calling `hangupCalls()` is triggered by the base `Account` when the state transitions to an error or unregistered state.

---

## Critical Code Paths

### Account Creation

```
Manager::addAccount(details)
  └── AccountFactory::createAccount(accountType, newId)
        ├── SIPAccount::SIPAccount(id, presenceEnabled)  [if type == "SIP"]
        └── JamiAccount::JamiAccount(id)                [if type == "RING"]
  └── account->setConfig(config)
  └── Manager::loadAccount(id)
        └── account->doRegister()
```

`AccountFactory` maintains per-type `AccountMap<Account>` maps under `mutex_`. The type registry is fixed at compile-time in `AccountFactory::AccountFactory()` (only `SIPAccount` and `JamiAccount` generators are registered).

### SIPAccount Registration

```
SIPAccount::doRegister()
  └── createTransport()                  ← selects UDP/TCP/TLS based on config
  └── sendRegister()
        └── pjsip_regc_create()          ← allocates PJSIP registrar context
        └── pjsip_regc_init()            ← sets registrar URI, From, Contact
        └── pjsip_regc_set_credentials() ← installs cred_[] (username/password/realm)
        └── pjsip_regc_send()            ← sends REGISTER; async, returns immediately
              └── [PJSIP thread] onRegCallback(pjsip_regc_cbparam*)
                    ├── code == 200 → setRegistrationState(REGISTERED)
                    ├── code == 401/403 → setRegistrationState(ERROR_AUTH)
                    └── code == 503/timeout → setRegistrationState(ERROR_*)
```

`sendRegister()` is the sole point where credentials are marshalled into PJSIP. The callback `onRegCallback` runs entirely on the PJSIP event thread.

### JamiAccount Registration (ArchiveAccountManager path)

```
JamiAccount::doRegister()
  └── setRegistrationState(INITIALIZING)
  └── doRegister_()                       ← private, called on dht io_context
        └── AccountManager::initAuthentication(key, deviceName, credentials,
                                               onSuccess, onFailure, onChange)
              └── ArchiveAccountManager::initAuthentication()
                    └── loadFromFile()    ← decrypt gzip archive (PBKDF2 + AES-GCM)
                          └── onSuccess callback → AccountInfo populated
        └── dht_->run(port, identity)     ← starts OpenDHT node
        └── dht_->bootstrap(nodes)        ← contacts well-known DHT bootstrap nodes
              └── [asio thread] onDhtConnected callback
                    └── setRegistrationState(REGISTERED)
        └── connectionManager_ created    ← dhtnet::ConnectionManager with TLS cert store
        └── startAccountPublish()         ← announces device presence on DHT
```

### JamiAccount Registration (ServerAccountManager path)

```
ServerAccountManager::initAuthentication()
  └── HTTP POST /api/auth/device (username, password, deviceName)
        └── [asio thread] response callback
              ├── on 200: receive signed device cert from JAMS CA
              │           → write cert to cert store
              │           → onSuccess callback
              └── on 401: onFailure(ERROR_AUTH)
  (same DHT bootstrap path follows after onSuccess)
```

### SIP TLS Handshake

```
SIPAccount::doRegister()
  └── createSipTransport()
        └── SipTransportBroker::getTlsTransport(tlsListener, remote)
              └── pjsip_tls_transport_connect() ← async TLS connect
                    └── [PJSIP thread] pjsip_transport_state_info callback
                          └── SipTransport::stateCallback(PJSIP_TP_STATE_CONNECTED, info)
                                └── TlsInfos populated (cipher, proto, verifyStatus, peerCert)
```

---

## Threading Model

| Operation | Thread | Notes |
|---|---|---|
| `Manager::loadAccount()`, `doRegister()` calls | Main thread (initial load) or ASIO io_context (API-triggered) | Multiple accounts loaded concurrently via ASIO |
| `JamiAccount::doRegister_()` and its asio callbacks | ASIO `io_context` thread pool | All DHT operations, connection manager events, conversation sync |
| `setRegistrationState()` (JamiAccount) | ASIO io_context thread | Triggers D-Bus signal emission on same thread |
| `SIPAccount::sendRegister()` | Called from ASIO context but immediately hands off to PJSIP | |
| `onRegCallback()` (SIPAccount) | **PJSIP event thread** (`sipThread_`) | Must not block; must complete quickly |
| `SipTransport::stateCallback()` | **PJSIP event thread** | TLS state transitions |
| `AccountFactory` map mutations | Any thread, protected by `AccountFactory::mutex_` | |
| `config_` read/write | Any thread, protected by `Account::configurationMutex_` (recursive mutex) | |
| `ArchiveAccountManager::AuthContext` | Any thread, protected by `AuthContext::mutex` | Async archive load; timer via `asio::steady_timer` |
| `ServerAccountManager` HTTP requests | ASIO io_context thread | Protected by `requestLock_` and `tokenLock_` |

---

## Key Operations with Expected Latency

| Operation | Expected Latency | Notes |
|---|---|---|
| SIP REGISTER round-trip (UDP, LAN) | 5–50 ms | Single REGISTER + 200 OK |
| SIP REGISTER round-trip (UDP, WAN) | 50–300 ms | |
| SIP TLS handshake + REGISTER | 100–600 ms | TLS adds one extra RTT + cert chain verify |
| DHT node bootstrap (warm, known peers) | 200 ms – 2 s | |
| DHT node bootstrap (cold start) | 2 s – 30 s | Peers may be behind NAT; ICE negotiation needed |
| Archive decrypt + load (`loadFromFile`) | 50–500 ms | PBKDF2 iterations dominate; hardware-dependent |
| JAMS REST authentication | 200 ms – 3 s | HTTPS + server processing |
| Certificate chain verification (GnuTLS) | 5–50 ms | Cached after first verify |
| Device announcement publish (DHT PUT) | 100 ms – 2 s | Async; does not block REGISTERED state |
| Device linking (addDevice full flow) | 5 s – 60 s | DHT discovery + ICE + TLS channel + archive transfer |

---

## Authentication

### SIPAccount
Credentials are stored in `SipAccountConfig` and marshalled into `std::vector<pjsip_cred_info> cred_` during `SIPAccount::loadConfig()`. The array is passed to `pjsip_regc_set_credentials()` before each `sendRegister()` call. TLS mutual authentication is optional: a client cert + key file path and CA cert file path can be configured; PJSIP loads them when creating the TLS transport factory (`TlsListener`).

### JamiAccount — ArchiveAccountManager
1. `AccountManager::initAuthentication()` generates a new ephemeral `PrivateKey` + `CertRequest` (PKCS#10 CSR) for this device session.
2. `ArchiveAccountManager::loadFromFile()` reads `archivePath_` (default: `~/.local/share/jami/<id>/archive`), decrypts with `computeKeys(password, pin)` → PBKDF2-SHA256 → AES-256-GCM.
3. Archive contains: account private key, account certificate, contacts msgpack, device list.
4. `AccountArchive` is deserialized; `AccountManager::info_` (`AccountInfo`) is populated: `id` (keypair), `deviceId`, `contacts` ptr.
5. Device cert is signed by the account key and installed into `dhtnet::CertificateStore`.

### JamiAccount — ServerAccountManager
1. `ServerAccountManager::initAuthentication()` sends `POST /api/auth/device` with `{username, password, deviceName, csr}` to JAMS (`managerHostname_`).
2. On 200: JAMS CA-signed device certificate is returned and stored.
3. Token (`token_`, `tokenScope_`, `tokenExpire_`) is cached; scope can be `Device`, `User`, or `Admin`.
4. JAMS CA certificate (`ServerAccountCredentials::ca`) is the trust anchor for subsequent TLS connections to JAMS endpoints.

---

## Coupling Map

```
account_management
    ├──[owns]──→ dht_layer
    │               JamiAccount::dht_ (dht::DhtRunner)
    │               JamiAccount::connectionManager_ (dhtnet::ConnectionManager)
    │               startAccountPublish() / startAccountDiscovery()
    │               DHT bootstrap on doRegister_()
    │
    ├──[uses]──→ connectivity
    │               SIPAccount creates SipTransport via SipTransportBroker
    │               JamiAccount creates IceTransport via dhtnet for each peer channel
    │               UPnP port mapping via Account::upnpCtrl_ (dhtnet::upnp::Controller)
    │
    ├──[uses]──→ certificate_pki
    │               dhtnet::tls::TrustStore — cert trust decisions
    │               dhtnet::CertificateStore — per-account cert cache
    │               JamiAccount::setCertificateStatus() / findCertificate()
    │               TlsInfos::peerCert in SipTransport (SIPAccount peer cert)
    │
    ├──[owns]──→ conversation_module
    │               JamiAccount::convModule_ (ConversationModule)
    │               JamiAccount::syncModule_ (SyncModule)
    │               Conversations loaded on REGISTERED state
    │
    ├──[signals to]──→ manager / client layer
    │               Account::setRegistrationState() emits
    │               libjami::ConfigurationSignal::RegistrationStateChanged
    │               AccountFactory called by Manager::addAccount() / removeAccount()
    │
    └──[persists via]──→ config / archive
                    AccountConfig (yaml-cpp) per account
                    AccountArchive (msgpack+GnuTLS AES-GCM) for JamiAccount
```

---

## Open Questions

1. What happens to in-flight calls when `setAccountActive(false)` is called? Is there a forced hang-up path?
2. How is the JAMS server URL validated — is there certificate pinning against a bundled CA or system store?
3. Can `AccountFactory` create custom account types from plugins, or is the type list fixed at compile time?
4. What is the retry strategy in `ArchiveAccountManager` when the archive file is corrupted or missing?
