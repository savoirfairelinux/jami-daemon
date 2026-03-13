# Config Persistence

## Status: draft

## Last Updated: 2026-03-13

---

## Purpose

The config persistence subsystem handles the serialization and deserialization of all user preferences and account configuration to and from disk. It uses YAML as the storage format (via yaml-cpp), a `Serializable` interface to standardize read/write behavior across all config classes, and provides template helpers (`yaml_utils`) for type-safe YAML node parsing. Global daemon preferences (audio device, ring tone, voicemail), per-account parameters, codec lists, and plugin preferences are all managed through this subsystem.

---

## Key Files

- `src/config/serializable.h` — `Serializable` interface: `serialize(YAML::Emitter&)` / `unserialize(const YAML::Node&)`
- `src/config/yamlparser.h` / `src/config/yamlparser.cpp` — `yaml_utils::parseValue`, `parseValueOptional`, `parsePath`, `parseVectorMap`, `parseVector` — YAML helper templates
- `src/config/account_config_utils.h` — per-account config utility helpers
- `src/preferences.h` / `src/preferences.cpp` — `Preferences` (global prefs), `VoipPreference`, `AudioPreference`, `PluginPreferences`, `VideoPreferences` — all implement `Serializable`
- `src/account_config.h` / `src/account_config.cpp` — `AccountConfig` base; `buildConfig()` / `setConfig()` / `loadConfig()` lifecycle
- `src/sip/sipaccount_config.h` / `.cpp` — `SipAccountConfig` (SIP-specific keys)
- `src/sip/sipaccountbase_config.h` / `.cpp` — `SipAccountBaseConfig`
- `src/jamidht/jamiaccount_config.h` / `.cpp` — `JamiAccountConfig` (DHT-specific keys)
- `src/archiver.h` / `src/archiver.cpp` — `Archiver`: reads/writes the top-level `jamidrc` YAML file; orchestrates account config save/load

---

## Key Classes

| Class | Role | File |
|---|---|---|
| `Serializable` | Interface with `serialize()` and `unserialize()`; all config classes implement it | `src/config/serializable.h` |
| `Preferences` | Global preferences: account order, network zone, registration expiry; implements `Serializable` | `src/preferences.h` |
| `VoipPreference` | VoIP behavior flags (playback, STUN, etc.); implements `Serializable` | `src/preferences.h` |
| `AudioPreference` | Audio device selection, ringtone path, echo cancellation settings; implements `Serializable` | `src/preferences.h` |
| `VideoPreferences` | Default video device and resolution; implements `Serializable` | `src/preferences.h` |
| `PluginPreferences` | Plugin enable/disable state; implements `Serializable` | `src/preferences.h` |
| `AccountConfig` | Per-account base config: accountId, alias, enabled flag, DTMF mode, codec list; implements `Serializable` | `src/account_config.h` |
| `SipAccountConfig` | SIP-specific config: registrar, credential list, STUN/TURN, NAT traversal | `src/sip/sipaccount_config.h` |
| `JamiAccountConfig` | JAMI-specific config: DHT bootstrap list, archive path, name server URL, turn usage | `src/jamidht/jamiaccount_config.h` |
| `Archiver` | Orchestrates reading/writing the top-level YAML config file (`jamidrc`); calls `serialize`/`unserialize` on all managed objects | `src/archiver.h` |

---

## External Dependencies

- **yaml-cpp** (`yaml-cpp/yaml.h`) — YAML parsing (`YAML::Node`) and emission (`YAML::Emitter`)
- `std::filesystem` — config file path resolution (XDG base dirs on Linux, `%APPDATA%` on Windows)
- No database; pure filesystem YAML

---

## Threading Model

- **Startup loading**: `Archiver::loadConfig()` is called once on the main thread during `Manager::init()` before any accounts or calls can be created; no concurrency concern.
- **Per-account config**: `Account::setConfig()` / `Account::editConfig()` acquire `configurationMutex_` (per-account `std::mutex`); safe to call from any thread.
- **Save-on-change**: `Manager::saveConfig()` is called on the io_context when settings change (e.g. after `setAccountDetails()`); synchronous file write on the io_context thread.
- No write-ahead log or atomic rename — a crash during write could corrupt the config file.

---

## Estimated Instrumentation Value

**Low.** Config reads and writes are infrequent (startup and explicit user changes). No tracing is needed beyond the existing `JAMI_DBG` calls on parse errors. The highest-value addition would be logging config file path and schema version at startup to aid bug reports.

---

## Open Questions

1. Is there a config file migration path when the YAML schema changes between daemon versions — are old unknown keys silently dropped?
2. Is the config file write atomic (write-then-rename) or in-place, risking corruption on crash?
3. Are account credentials (SIP password, archive encryption key) stored in the YAML config or separately secured?
4. Is there a maximum config file size or account count that impacts YAML parse time at startup?
5. Does `Archiver` handle the case where the config directory is read-only (e.g. enterprise policy)?
