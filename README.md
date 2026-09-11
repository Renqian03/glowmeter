# Glowmeter — Project 2

A phone web app that sets an ESP32-S3's onboard LED and shows a live reading, over WiFi. Built for **Project 2** of *Basic Skills for Experimentalists* (TIGP, 2026).

**Project site:** https://renqian03.github.io/glowmeter/ (pitch, screenshot, protocol notes)
**Try it now (demo mode, no board required):** https://renqian03.github.io/glowmeter/app/

## Layout

| Path | What |
|---|---|
| `firmware/` | Unmodified baseline firmware from [`class-board-2026`](https://github.com/TIGP-Experimental-Methods/class-board-2026) — PlatformIO project, Arduino/ESP32-S3. Already implements everything this app needs (`led`, `brightness`, `status` broadcast); no firmware changes were required. |
| `host/pwa/` | **The app itself** — plain HTML/JS/CSS, no build step, no framework. This is the source of truth; it gets copied into `firmware/data/` at build time and flashed onto the board's LittleFS filesystem, so the board serves it locally over plain HTTP. |
| `host/instrument.py` | The class's Python CLI client, for testing the board's WebSocket from a laptop without the phone app. |
| `app/` | A mirror of `host/pwa/` published to GitHub Pages, so the app can be tried online (in demo mode) without a board. |
| `index.html`, `images/` | The project site (this repo's GitHub Pages root). |

## Flashing it onto your board

```
cd firmware
pio run -e esp32s3-sim -t upload      # firmware (use esp32s3 for the real class board)
pio run -e esp32s3-sim -t uploadfs    # the web app (host/pwa/), built into LittleFS
```

Then open `http://<board-ip>/` (AP mode: `http://192.168.4.1/`, or `http://instrument.local/` on the class WiFi) on your phone, on the same network as the board.

## Protocol

JSON over one WebSocket at `ws://<board>/ws`. See [`firmware/PROTOCOL.md`](firmware/PROTOCOL.md) for the full spec (inherited unmodified from `class-board-2026`); Glowmeter uses only the `base` block's `led`, `brightness`, and `status` messages, plus `b1.ai1…ai8` as an optional measurement source when the ADS8688 input block is attached.

---
🤖 Built with [Claude Code](https://claude.com/claude-code)
