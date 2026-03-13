# Plugin System

## Status: draft

## Last Updated: 2026-03-13

---

## Purpose

The plugin system allows third-party native libraries to extend libjami's functionality at runtime without recompiling the daemon. Plugins are packaged as `.jpl` archives (zip files containing a manifest, a shared library, and assets), cryptographically signed by the author, and installed via the `JamiPluginManager` API. Once loaded, a plugin can register handlers for four hook types: **call media** (intercept audio/video frames), **chat** (intercept/decorate messages), **webview** (inject UI into the in-call webview), and **preferences** (add custom settings pages). The system validates plugin certificates against OpenDHT cryptographic primitives before loading.

---

## Key Files

- `src/plugin/jamipluginmanager.h` / `.cpp` — `JamiPluginManager`: top-level manager exposed to config API
- `src/plugin/pluginmanager.h` / `.cpp` — `PluginManager`: low-level load/unload registry
- `src/plugin/pluginloader.h` / `.cpp` — `PluginLoader`: dlopen/FreeLibrary wrapper
- `src/plugin/callservicesmanager.h` / `.cpp` — `CallServicesManager`: call media hook dispatch
- `src/plugin/chatservicesmanager.h` / `.cpp` — `ChatServicesManager`: chat hook dispatch
- `src/plugin/preferenceservicesmanager.h` / `.cpp` — `PreferenceServicesManager`: preference hook dispatch
- `src/plugin/webviewservicesmanager.h` / `.cpp` — `WebViewServicesManager`: webview hook dispatch
- `src/plugin/pluginpreferencesutils.h` / `.cpp` — preference value persistence utilities
- `src/plugin/pluginsutils.h` / `.cpp` — archive extraction, manifest parsing, path helpers
- `src/plugin/jamiplugin.h` — `JamiPlugin` loaded-plugin descriptor
- `src/plugin/mediahandler.h` — `MediaHandler` interface for audio/video frame hooks
- `src/plugin/chathandler.h` — `ChatHandler` interface
- `src/plugin/preferencehandler.h` — `PreferenceHandler` interface
- `src/plugin/webviewhandler.h` — `WebViewHandler` interface, `WebViewMessage`
- `src/plugin/streamdata.h` — `StreamData` (passed to media hooks: call-id, stream direction, codec info)
- `src/plugin/store_ca_crt.cpp` — bundled CA certificate for plugin certificate validation
- `src/jami/plugin_manager_interface.h` — public API: `loadPlugin`, `unloadPlugin`, `installPlugin`, `getInstalledPlugins`

---

## Key Classes

| Class | Role | File |
|---|---|---|
| `JamiPluginManager` | Public façade; certificate validation, manifest parsing, install/uninstall, preference storage | `src/plugin/jamipluginmanager.h` |
| `PluginManager` | Registry of loaded plugins; calls `PluginLoader` to dlopen; manages plugin capability map | `src/plugin/pluginmanager.h` |
| `PluginLoader` | Platform abstraction for `dlopen`/`dlsym`/`dlclose` (Linux) and `LoadLibrary` (Windows) | `src/plugin/pluginloader.h` |
| `CallServicesManager` | Injects media handler hooks into active `SIPCall` streams for each loaded plugin | `src/plugin/callservicesmanager.h` |
| `ChatServicesManager` | Calls registered `ChatHandler` hooks on incoming/outgoing messages | `src/plugin/chatservicesmanager.h` |
| `PreferenceServicesManager` | Manages `PreferenceHandler` registrations; persists per-plugin preference values | `src/plugin/preferenceservicesmanager.h` |
| `WebViewServicesManager` | Routes `WebViewMessage` events between the in-call webview and plugins | `src/plugin/webviewservicesmanager.h` |
| `StreamData` | Carries stream context (callId, mediaType, direction) to media hooks | `src/plugin/streamdata.h` |

---

## External Dependencies

- **OpenDHT crypto** (`opendht/crypto.h`) — `dht::crypto::Certificate` used for plugin certificate validation
- **dlopen / FreeLibrary** — dynamic library loading (Linux / Windows)
- **minizip / libarchive** (implied by `.jpl` extraction in `pluginsutils`) — archive extraction
- No sandboxing runtime; plugin code runs in-process with full daemon privileges

---

## Threading Model

- **Install/uninstall**: runs on the ASIO io_context (initiated via `JamiPluginManager` public methods called from Manager).
- **Media hooks** (`CallServicesManager`): called synchronously on the encoding/decoding `ThreadLoop` thread that processes each frame — hooks must be fast and non-blocking.
- **Chat hooks** (`ChatServicesManager`): called on the thread that processes incoming messages (ASIO io_context or PJSIP thread depending on account type).
- **Preference hooks**: called on the io_context.
- No inter-plugin locking; each plugin is responsible for its own thread safety.

---

## Estimated Instrumentation Value

**Low.** Plugin load/unload is infrequent. Certificate validation failures and hook dispatch errors are worth logging. Tracing individual frame-level media hook invocations would create excessive overhead; aggregate counters per hook type are preferable.

---

## Open Questions

1. Is there a defined ABI stability contract for the plugin APIs (handler interfaces) across daemon versions?
2. Can a plugin be hot-reloaded (unload + reload without daemon restart)?
3. Are media hooks invoked for conference streams in addition to point-to-point call streams?
4. What happens if a plugin's media hook throws an exception — is the exception caught, or does it propagate and crash the media thread?
5. Is the bundled CA in `store_ca_crt.cpp` the only trust anchor for plugin certs, or can users add their own?
