# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Requires Qt 6 (Widgets, Network, WebSockets). On macOS with Homebrew:

```bash
# Configure (from repo root)
cmake -B cmake-build-debug -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build cmake-build-debug

# Run
./cmake-build-debug/SamsungTvRemote
```

CLion manages the build automatically via its CMake integration. The `cmake-build-debug/` directory is the IDE's default output directory and should not be committed (it has its own `.gitignore`).

There are no tests in this project.

## Architecture

Four source files in `src/`:

- **`TvDevice.h`** — Plain struct holding discovered TV metadata (name, IP, model, ID) with a `displayName()` helper. No Qt signals.
- **`SsdpDiscovery`** — Sends a single SSDP M-SEARCH multicast on port 1900, filters responses for Samsung/MediaRenderer/MainTVAgent keywords, then fetches `http://<ip>:8001/api/v2/` to enrich each device. Emits `deviceFound(TvDevice)` per TV and `finished()` after a fixed timeout (default 3500 ms).
- **`SamsungRemote`** — Wraps a single `QWebSocket`. Connects to `wss://<ip>:8002/api/v2/channels/samsung.remote.control` first; if that fails it retries on `ws://<ip>:8001` (Samsung TVs use self-signed TLS, so SSL errors are always ignored). Receives a pairing token on `ms.channel.connect` and surfaces `ms.channel.unauthorized` as a `pairingRequired()` signal. Keys are sent as JSON `ms.remote.control` messages.
- **`MainWindow`** — Owns both `SsdpDiscovery` and `SamsungRemote` as members. The remote button grid is wrapped in a `QScrollArea` (it's taller than most screens) below a toggleable connection/setup panel. Persists the last-used TV IP and per-IP pairing tokens via `QSettings` (org: `SatishLabs`, app: `SamsungTvRemote`).

## Remote layout (Full vs Compact)

The button grid shown in the scroll area is swappable at runtime between two builders:

- **`buildFullRemotePanel()`** — the original, full-featured layout: transport controls (rewind/stop/FF), colour keys, and a 2×2 app-shortcut grid, in addition to what's below.
- **`buildCompactRemotePanel()`** — a trimmed "One Remote"-style layout modeled on Samsung's modern compact remotes: power, `123`/menu/guide, the D-pad, back/home/play-pause, and 3 app shortcuts in a single row. No transport-control row, no CC/AD, no colour keys.
- Both layouts share the same `buildRocker()` (top/middle/bottom, vertical) for VOL and CH: middle button labeled "VOL"/"CH", sending `KEY_MUTE`/`KEY_PRECH` respectively — matching what the middle of a real Samsung rocker does.

The choice lives in the "Layout:" row of the connection/setup panel (`layoutCombo_`) and is persisted as `ui/compactLayout` in `QSettings`, read back in `buildUi()` to pick the initial panel. Switching at runtime (`setRemoteLayout()`) must clear `remoteButtons_` and re-derive it from the freshly built panel's buttons before deleting the old widget (`remoteScrollArea_->takeWidget()`) — those pointers would otherwise dangle, since `setRemoteEnabled()` iterates `remoteButtons_` on every connect/disconnect.

Compact mode's app shortcuts reuse existing, verified Samsung key names (`KEY_NETFLIX`, `KEY_AMAZON`, `KEY_SAMSUNGTVPLUS`) rather than the Rakuten TV button shown on some physical compact remotes, since no verified `KEY_*` code for Rakuten exists in this codebase.

## Connecting to a TV

`SamsungRemote::connectToTv()` is a real WebSocket handshake to the TV's remote-control API — **not** a ping/reachability probe. It has real side effects:

- It performs an actual TCP connect + TLS/WS upgrade to `wss://<ip>:8002/...`, falling back to `ws://<ip>:8001` once on failure (see Connection fallback below). Against an unreachable IP this blocks on a real TCP timeout (several seconds), which is why connecting to a stale/offline saved TV isn't instant.
- If the TV doesn't already know this app (no valid saved token), connecting makes the TV show a real on-screen "Allow this app to connect?" pairing prompt that must be approved with the physical remote — surfaced here as `pairingRequired()`. With a valid saved token it connects silently, no prompt.
- No `KEY_*` command is ever sent as part of connecting. `sendKey()` is only invoked when a remote button is actually pressed, so opening/failing a connection never operates the TV.

**Startup auto-connect** (`MainWindow` constructor): if a TV is remembered in `QSettings`, the app hides the setup panel and immediately calls `connectSelectedTv()` for it (tracked via `autoConnecting_`). If nothing is remembered, or that auto-connect's `errorOccurred` fires, the setup panel is shown and `SsdpDiscovery` is started so the user can find/pick a TV — without popping the usual error dialog (that dialog is reserved for explicit, user-initiated `Connect` clicks).

**Relocation self-heal**: Samsung's `/api/v2/` response includes a stable per-TV `id` (a UUID) that survives DHCP/IP changes, unlike the socket address. `MainWindow` persists this as `selectedTv/id` alongside the IP. When `SsdpDiscovery` reports a device whose `id` matches the remembered one but whose IP differs, `addDevice()` treats it as the same TV having moved (`handleRememberedTvRelocated()`): it corrects `selectedTv/ip` in `QSettings` and silently reconnects, rather than listing it as a separate, unrecognized entry. This only fires once discovery has actually run and learned the TV's `id` — a direct auto-connect that succeeds at the saved IP never triggers discovery, so `id` stays unknown until the first time the TV becomes unreachable (or discovery is run manually). Practically: the *first* IP change after adopting this still needs one manual reconnect; every IP change after that self-heals.

## Persistence

`MainWindow` is the only class that persists anything, and `QSettings` (default-constructed, so it uses the org/app name set once in `main.cpp`: org `SatishLabs`, app `SamsungTvRemote`) is the only mechanism — no cache files, logs, or window-geometry saving anywhere in the codebase. On macOS that resolves to `~/Library/Preferences/com.satishlabs.SamsungTvRemote.plist` (Windows: `HKEY_CURRENT_USER\Software\SatishLabs\SamsungTvRemote`; Linux: `~/.config/SatishLabs/SamsungTvRemote.conf`).

| Key | Holds | Written by | Read by |
|---|---|---|---|
| `selectedTv/ip` | Remembered TV's IP | `saveSelectedTv()`, `handleRememberedTvRelocated()` | `loadSettings()` |
| `selectedTv/name` | Its display name | same | same |
| `selectedTv/model` | Its model string | same | same |
| `selectedTv/id` | Its stable Samsung UUID (see Relocation self-heal below) | `saveSelectedTv()` | `loadSettings()` |
| `tokens/id/<id>` | Pairing token, keyed by stable id (preferred) | `storeToken()` | `tokenFor()` |
| `tokens/<ip>` | Pairing token, keyed by IP (legacy fallback, and for manually-entered IPs with no known id) | `storeToken()` | `tokenFor()` |
| `ui/compactLayout` | Full vs. Compact remote layout choice | `setRemoteLayout()` | `buildUi()` |

Note there's no persisted history of *discovered* TVs — only the one currently "remembered" TV and its token. Every `SsdpDiscovery` scan starts from scratch each run.

## Key design notes

- **Token persistence**: see the Persistence section above — tokens are id-keyed when possible since that survives IP changes, with an IP-keyed fallback. On reconnect the resolved token is passed as the WebSocket query parameter so the TV skips the on-screen approval prompt.
- **Connection fallback**: `SamsungRemote::triedInsecure_` gates one retry from port 8002 (WSS) to 8001 (WS) on any socket error. After that, the error is surfaced to the UI.
- **No `.ui` files**: All UI is constructed in code inside `MainWindow::buildUi()` and its helpers. There is no Qt Designer file.
- **`CMAKE_AUTOMOC ON`**: Qt's MOC is run automatically by CMake; no manual `moc_*` steps are needed.
