# CYD WebDeck (No-Touch)

A browser-controlled dashboard firmware concept for **ESP32 CYD boards without touchscreen input**.

## What this repo includes

- A concrete, no-touch-oriented feature spec.
- A starter firmware sketch in `firmware/CYD_WebDeck_NoTouch/CYD_WebDeck_NoTouch.ino`.
- A module-first architecture so features can be activated dynamically to save RAM.

## No-touch design principles

Because there is no on-device input, control is fully remote:

1. Device boots, connects to Wi-Fi, and starts a web control panel.
2. Screen always shows current mode + status.
3. Any mode can be changed from browser instantly.
4. Device auto-returns to standby after inactivity.
5. WebSocket channel pushes live updates (mode changes, logs, notifications).

## Target hardware

- ESP32-WROOM-32
- ILI9341 (240x320) over SPI
- Optional SD card (for slideshow assets)

## Core stack

- `WiFi`
- `ESPAsyncWebServer` + `AsyncTCP`
- `WebSocketsServer`
- `TFT_eSPI`
- `ArduinoJson`

## Screen modes

- Standby / Clock
- Weather
- Crypto
- System Stats
- Terminal Viewer
- Notifications
- Image Slideshow

## Browser control panel (starter API)

- `GET /` returns a simple control page.
- `POST /api/mode` sets active screen mode.
- `POST /api/notify` shows a notification on screen.
- `POST /api/brightness` updates display brightness value.
- `GET /api/state` returns JSON system state.
- WebSocket (`/ws`) receives instant updates and terminal lines.


## Arduino IDE compile fix (for `invalid preprocessing directive #CYD`)

If you see:

```
error: invalid preprocessing directive #CYD
```

it means a Markdown heading (from `README.md`) was pasted/opened as an `.ino` sketch.

Use this file as the sketch source instead:

- `firmware/CYD_WebDeck_NoTouch/CYD_WebDeck_NoTouch.ino`

Quick check: line 1 of the `.ino` must start with `#include`, not `# CYD ...`.

## Library setup (fix for `ESPAsyncWebServer.h: No such file or directory`)

If Arduino IDE shows:

```
fatal error: ESPAsyncWebServer.h: No such file or directory
```

install these libraries in **Library Manager** (or equivalent):

- `ESP Async WebServer`
- `AsyncTCP`
- `ArduinoJson`
- `TFT_eSPI`
- `WebSockets` (Links2004)

This sketch now supports a fallback path:

- If `ESPAsyncWebServer` + `AsyncTCP` are installed, it uses async HTTP + `/ws`.
- If they are missing, it automatically compiles with built-in `WebServer` and keeps the legacy websocket on port `81`.


## Suggested build-out order

1. Wi-Fi config + captive portal (optional).
2. Stable display manager + mode renderer.
3. Weather/Crypto API clients with cache.
4. Slideshow file indexing and image decode.
5. Web UI polish and auth token.

## Notes

The starter sketch intentionally favors clarity over production hardening. Before deployment, add:

- Authentication for control endpoints.
- Non-volatile config storage (`Preferences` or LittleFS).
- Rate-limits and payload validation.
- Better task isolation for network I/O vs. display drawing.
