# Message protocol (JSON over WebSocket)

One WebSocket at `ws://<board>/ws` (`instrument.local` or `192.168.4.1`). All messages are UTF-8 JSON text frames. Three message kinds flow over it.

## 1. Request → reply (client to board)

```json
{"id": 1, "block": "base", "cmd": "led", "args": {"r": 255, "g": 0, "b": 0}}
```

| Field | Meaning |
|---|---|
| `id` | any number/string; echoed in the reply so the client can match them |
| `block` | block name: `base`, `b1`…`b5`, `template`, `alarms` |
| `cmd` | command name, block-specific (see §4) |
| `args` | object, may be omitted |

Reply, always exactly one per request:

```json
{"id": 1, "ok": true,  "result": {"r": 255, "g": 0, "b": 0}}
{"id": 1, "ok": false, "error": "unknown cmd"}
```

Requests are handled in the firmware's `loop()`, in order, one at a time.

## 2. Broadcasts (board to every client)

**`hello`** — sent once when a client connects; lists the registered blocks so the app can build its tabs.

```json
{"type": "hello", "fw": "0.1.0", "sim": true, "blocks": ["base", "b1", "b2", "b3", "b4", "b5", "template", "alarms"]}
```

**`status`** — every 50 ms (20 Hz). `t` is the board's `millis()`.

```json
{"type": "status", "t": 123456,
 "blocks": {"base": {"counter": 1234, "temp_c": 41.2, "uptime_s": 123, "rssi": 0, "heap_free": 250000,
                     "clients": 1, "led": {"r": 0, "g": 0, "b": 40, "brightness": 40}},
            "b1": {"ai1": 3.91, "ai2": -1.2, "...": 0, "range": 0},
            "b4": {"relay1": false, "opto1": 17, "...": 0}}}
```

Numeric top-level keys inside a block (`base.counter`, `b1.ai1`, …) are what the live chart can plot and what alarm rules can test. Nested objects (`base.led`) are for display only.

**`alarm`** — when a rule fires (rising edge).

```json
{"type": "alarm", "t": 123456, "rule": 2, "block": "b1", "key": "ai1", "value": 9.4, "action": "relay:1:off"}
```

## 3. HTTP

`GET /api/info` → `{"fw","sim","mac","blocks":[...]}` (same as `hello`, for scripts without WebSocket). Everything else under `/` is the web app served from LittleFS.

## 4. Commands per block

| Block | `cmd` | `args` | `result` |
|---|---|---|---|
| `base` | `led` | `{r,g,b}` 0–255 | `{r,g,b}` |
| | `brightness` | `{value}` 0–255 | `{brightness}` |
| | `counter_reset` | — | `{counter:0}` |
| | `info` | — | `{fw, chip, mac, ip, sim}` |
| `b1` | `read_all` | — | `{ai:[8 volts]}` |
| | `set_range` | `{ch:1..8, range:0..6}` | `{ch, range}` |
| `b2` | `rails` | — | `{v5_raw, v3v3, v12p, v12n, v5a, measured}` |
| `b3` | `set_dc` | `{ch:1..2, volts}` | `{ch, mode}` |
| | `sine` | `{ch, freq, amp, offset}` | `{ch, mode}` |
| | `off` | `{ch}` | `{ch, mode}` |
| `b4` | `relay` | `{n:1..4, on}` | `{n, on}` |
| | `relay_all` | `{on}` | `{on}` |
| | `opto_reset` | — | `{}` |
| `b5` | `dio` | `{n:1..8, level}` | `{mask}` |
| | `dio_mask` | `{mask:0..255}` | `{mask}` |
| | `trig_dir` | `{out}` | `{out}` |
| | `trig` | `{level}` (only when `out`) | `{level}` |
| | `fast_out` | `{n:1..2, freq_hz}` (0 = off) | `{n, freq_hz}` |
| `template` | `set_value` | `{value}` | `{setpoint}` |
| `alarms` | `list` | — | `{rules:[...]}` |
| | `add` | `{block, key, op, threshold, action}` | the new rule incl. `id` |
| | `remove` | `{id}` | `{removed}` |
| | `clear` | — | `{}` |

Alarm rule fields: `op` ∈ `gt lt ge le eq ne`; `action` = `notify` or `relay:<n>:on` / `relay:<n>:off`. Rules fire once per rising edge and are saved to `/alarms.json` on the board.

## 5. Status keys per block

`base`: counter, temp_c, uptime_s, rssi, heap_free, clients, led{r,g,b,brightness} ·
`b1`: ai1…ai8, range · `b2`: v5_raw, v3v3, v12p, v12n, v5a, measured ·
`b3`: ao1, ao2, mode1, mode2 · `b4`: relay1…relay4, opto1, opto2, opto1_level, opto2_level ·
`b5`: dio, dio1…dio8, trig_dir, trig, fast1_hz, fast2_hz · `template`: value, setpoint · `alarms`: rules, active

## 6. Streaming and capture (Decision #26, 2026-09-06 — specified, not yet implemented)

The 20 Hz `status` broadcast is the slow channel. Fast data uses **binary WebSocket frames** on the same socket, in two modes that every real oscilloscope has:

**Rolling (continuous stream).** `{"id":7,"block":"b1","cmd":"stream","args":{"ch":1,"rate_hz":10000,"chunk":500}}` starts a stream; the board then sends one binary frame per `chunk` samples (here every 50 ms) until `{"cmd":"stream","args":{"ch":1,"rate_hz":0}}`. The app appends frames to a rolling buffer and draws the last N ms — a chart recorder / scope in roll mode.

**Triggered (burst capture).** `{"id":8,"block":"b1","cmd":"capture","args":{"ch":1,"rate_hz":100000,"n":4000,"trig":{"level":1.0,"edge":"rising","pre":1000}}}` arms a capture: the board samples into RAM (DMA), waits for the trigger crossing, keeps `pre` samples before it, and sends **one** binary frame with all `n` samples. `trig` omitted = capture immediately.

**Binary frame layout** (little-endian): `uint8 kind` (1 = stream chunk, 2 = capture) · `uint8 block_id` · `uint8 ch` · `uint8 bits` (12 or 16) · `uint32 t_ms` of the first sample · `uint32 rate_hz` · `uint32 n` · `int32 trig_index` (−1 if none) · `float32 volts_per_lsb` · `float32 offset_v` · then `n × int16` raw samples. Total 28-byte header + 2 n bytes; 4000 samples = 8 kB, which WiFi moves many times per second.

**Sources and realistic limits:** the dev board's internal ADC (`base`, 12-bit, noisy, GPIO 1–10) sustains a few kS/s rolling and ~80 kS/s burst via ADC continuous/DMA; the class board's ADS8688 (`b1`, 16-bit, ±10 V) gives tens of kS/s per channel rolling and up to 500 kS/s aggregate in burst. Rolling streams are capped by the WiFi link (budget ≈ 200 kB/s → ≈ 100 kS/s of int16 total across all clients); the board refuses `rate_hz` above the cap with `ok:false`.

**Where it lives:** the sampler + frame encoder are instructor-owned in `base` (dev-board ADC, so it works from Workshop 1 in sim and on the bare board); `b1` reuses the encoder with the ADS8688 (block owner extends it in E11). The PWA gets a **Scope** tab: roll / single / auto-trigger, timebase, cursor, "download CSV" of the visible buffer (Web Share on phones). `instrument.py` gets `scope` (stream to CSV/NumPy) and `capture`.

**Same socket, no extra ports:** text frames stay JSON as above; the app tells them apart by frame type (`typeof data === "string"`).
