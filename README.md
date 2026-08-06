# Samsung TV Remote — Qt 6 / CLion

A Qt Widgets desktop remote for Samsung Tizen TVs. It discovers TVs over SSDP, remembers the selected TV and pairing token, and sends remote-control keys over Samsung's WebSocket interface.

## Features

- SSDP/UPnP TV discovery and manual IP entry
- Saved TV selection and pairing token via `QSettings`
- Secure WebSocket on port 8002 with port 8001 fallback
- Remote-shaped dark interface
- Power off, Menu, Source, Info, Guide and 123 panel
- Direction pad and OK/Enter
- Home, Back and Play/Pause
- Rewind, Stop and Fast Forward
- Volume Up/Down/Mute
- Channel Up/Down and Previous Channel
- Accessibility/Audio Description key
- Red, Green, Yellow and Blue function keys
- Netflix, Prime Video, Disney+ and Samsung TV Plus shortcut key attempts

## Build

Install Qt 6, then set CLion's CMake option to the Qt prefix if required:

```text
-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
```

Build requirements are Qt 6 Widgets, Network and WebSockets.

## First connection

1. Put the computer and TV on the same LAN.
2. Turn the TV on.
3. Run the application and discover/select the TV.
4. Select **Connect and remember**.
5. Approve the connection prompt shown on the TV.

## Compatibility notes

Samsung does not publish this WebSocket remote interface as a stable general-purpose desktop API. Key support varies by model, region, installed applications and firmware.

- `KEY_POWER` generally powers an already-connected TV off. Powering a TV on normally requires Wake-on-LAN and the TV's MAC address, and is not supported by every model/configuration.
- Voice/microphone operation on the physical remote cannot generally be reproduced by merely sending a WebSocket key because it also involves voice capture and Samsung services.
- Dedicated app key names are firmware-dependent. Unsupported shortcut buttons may do nothing. Home navigation remains the portable way to launch apps.
- Some TVs map CC/AD, Guide, Menu, colour keys or media keys differently.
