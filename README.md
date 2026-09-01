# Samsung TV Remote — Qt 6 / CLion

A Qt Widgets desktop remote for Samsung Tizen TVs. It discovers TVs over SSDP, remembers the selected TV and pairing token, auto-connects to it on launch, and sends remote-control keys over Samsung's WebSocket interface.

## Features

- **Auto-connect on launch** — if a TV is remembered, the app connects to it immediately and keeps the setup panel out of the way. If nothing is remembered yet, or the remembered TV can't be reached, the setup panel opens and SSDP discovery runs automatically so you can find/pick one.
- **Self-heals when your TV's IP changes** — Samsung TVs expose a stable id that survives DHCP/IP changes. Once discovery has learned it, a future IP change is detected and reconnected automatically, no manual re-pairing. (The very first IP change after upgrading, or on a TV never seen by discovery, still needs one manual reconnect — see [Compatibility notes](#compatibility-notes).)
- SSDP/UPnP TV discovery and manual IP entry
- Saved TV selection and pairing token via `QSettings`, keyed by the TV's stable id where known (falling back to its IP for manually-entered TVs)
- Secure WebSocket on port 8002 with port 8001 fallback
- **Two selectable remote layouts**, chosen from the **Layout:** dropdown in TV setup and remembered across restarts:
  - **Full** — transport controls (rewind/stop/FF), 3-segment VOL/CH rockers (middle button mutes / jumps to the previous channel), colour keys, and 4 app shortcuts (Netflix, Prime Video, Disney+, Samsung TV Plus)
  - **Compact** — a trimmed "One Remote"-style layout: power, `123`/menu/guide, D-pad, back/home/play-pause, the same VOL/CH rockers, and 3 app shortcuts (Netflix, Prime Video, Samsung TV Plus)
- Remote-shaped dark interface; the button grid scrolls instead of squeezing/overlapping on shorter screens
- Direction pad and OK/Enter
- Home, Back and Play/Pause
- Volume Up/Down/Mute and Channel Up/Down/Previous Channel
- Accessibility/Audio Description key (Full layout)
- Red, Green, Yellow and Blue function keys (Full layout)

## Build

Install Qt 6, then set CLion's CMake option to the Qt prefix if required:

```text
-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
```

Build requirements are Qt 6 Widgets, Network and WebSockets.

## First connection

1. Put the computer and TV on the same LAN.
2. Turn the TV on.
3. Run the application — since no TV is remembered yet, the setup panel opens and discovery starts automatically.
4. Select the discovered TV (or enter its IP manually) and choose **Connect and remember**.
5. Approve the connection prompt shown on the TV.

From then on, launching the app auto-connects to that TV and the setup panel stays hidden — you'll only see it again if the TV can't be reached, or you open it yourself via **TV setup**.

## Switching remote layouts

Open **TV setup** and use the **Layout:** dropdown to switch between **Full remote** and **Compact remote**. The switch happens immediately (no restart needed) and is remembered for next time.

## Compatibility notes

Samsung does not publish this WebSocket remote interface as a stable general-purpose desktop API. Key support varies by model, region, installed applications and firmware.

- `KEY_POWER` generally powers an already-connected TV off. Powering a TV on normally requires Wake-on-LAN and the TV's MAC address, and is not supported by every model/configuration.
- Voice/microphone operation on the physical remote cannot generally be reproduced by merely sending a WebSocket key because it also involves voice capture and Samsung services.
- Dedicated app key names are firmware-dependent. Unsupported shortcut buttons may do nothing. Home navigation remains the portable way to launch apps.
- Some TVs map CC/AD, Guide, Menu, colour keys or media keys differently.
- The Play/Pause button sends `KEY_PLAY_PAUSE`, which isn't part of Samsung's documented key set (only separate `KEY_PLAY`/`KEY_PAUSE` exist) — on real hardware it's currently a no-op. `OK` (`KEY_ENTER`) happens to pause video in apps like Netflix because those apps treat a select press during playback as toggle-pause, not because it's a dedicated pause command.
- The TV's IP-change self-heal only works once discovery has actually run and learned the TV's id — a direct auto-connect that succeeds at the saved IP never triggers discovery, so a TV that's never had its IP change since first pairing won't be "known" for self-heal purposes until discovery runs at least once (e.g. after the first time it does become unreachable).
- To review or revoke this app's access on the TV itself: **Settings → General → External Device Manager → Device Connection Manager → Device List**.

## License

This project is licensed under the MIT License.

Copyright © 2026 Satish Kunapuli.
See the [LICENSE](LICENSE) file for details.