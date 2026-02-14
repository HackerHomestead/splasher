# Splasher completion plan

This plan covers the README TODO list plus inline code TODOs, ordered by dependency and impact.

---

## Phase 1: Clean up & harden current SPI read path

**Goal:** Make the existing dump flow correct, configurable, and maintainable before adding protocols or write support.

| # | Task | Where | Notes |
|---|------|--------|------|
| 1.1 | **Wire speed (`-s`) to SPI timing** | `hardware.cpp` `setTiming()`, `main.cpp` | `convertKHz()` is used but `setTiming(0)` is hardcoded in `dumpFlashToFile()`. Pass `dev.KHz` into `setTiming()`; implement KHz → `gpioDelay()` (see `doc/notes.txt`: 1 µs min, ~1 MHz max). |
| 1.2 | **Support read offset** | `Device.offset`, `dumpFlashToFile()` | `Device` has `offset`; address is currently forced to 0. Send 3-byte address from `dev.offset` in the Read (0x03) sequence. |
| 1.3 | **Fix pinout consistency** | `hardware.cpp` | `dumpFlashToFile()` uses `hwSPI(2, 3, 4, 14, 15)` but README says CS=27, HOLD=17, WP=22. Use one canonical pinout (e.g. from README) in one place (config or constant). |
| 1.4 | **Add Write Protect control** | `hwSPI` in `hardware.hpp/cpp` | Implement enable/disable so WP pin can be driven for write operations later. |
| 1.5 | **Small main.cpp cleanups** | `main.cpp` | (1) Proper error message and `exit(EXIT_FAILURE)` on `gpioInitialise()` fail. (2) Optionally move `BinFile*` into `main` scope. (3) Fill in long help “By default .......TODO”. (4) Either remove or gate “Speed not specified” message (e.g. only when verbose). |
| 1.6 | **Optional: bytes auto-detect** | `main.cpp` + device layer | If `-b` omitted, could use JEDEC ID (Phase 2) to infer size; otherwise keep “bytes required” and document. |

**Exit criteria:** `splasher output.bin -b 16M -s 100` dumps 16 MiB at 100 KHz; offset works; pinout matches README; no hardcoded timing in dump path.

---

## Phase 2: JEDEC ID & protocol abstraction

**Goal:** Identify chips and lay the groundwork for multiple protocols and “init read / init write”.

| # | Task | Where | Notes |
|---|------|--------|------|
| 2.1 | **Add common limits namespace** | `hardware.hpp` (or new `limits.hpp`) | Centralize max bytes (256 MiB), speed limits, buffer sizes, etc. |
| 2.2 | **Implement JEDEC ID read (SPI)** | `hwSPI` + new protocol layer | Send 0x9F, read 3 bytes (manufacturer, memory type, capacity). Store in `Device` or a small “ChipId” struct. |
| 2.3 | **Protocol-specific command bytes** | `hardware.hpp` “add protocol specific bytes” | Define per-protocol command constants (e.g. Read, Write, Sector Erase for 25-series). Use in one place so DSPI/QSPI can share. |
| 2.4 | **“Init read / init write”** | New API or `splasher::` functions | Init read: power-on/GPIO init + JEDEC read + optional size/offset from device. Init write: WP disable + optional erase. Can be separate functions called before dump/flash. |
| 2.5 | **Inherited class members (for expansion)** | `hardware.hpp/cpp` | Introduce a base (e.g. `hwInterface` or per-protocol base) with `readByte()`, `writeByte()`, `readId()` so 25-series SPI, then DSPI/QSPI/I2C, can share a common interface and “readBytes”-style loop. |

**Exit criteria:** JEDEC ID reported; dump uses protocol-specific commands from a single place; init read/write entry points exist; structure ready for multiple interfaces.

---

## Phase 3: Write path (flash / erase)

**Goal:** Support writing from file to flash and erase, so “flash, dump, clone and erase” is real.

| # | Task | Where | Notes |
|---|------|--------|------|
| 3.1 | **BinFile read path** | `filemanager.hpp/cpp` | Add way to read bytes from file into buffer (e.g. `pullByteFromFile()` or read chunk into `byteArrayPtr` and serve bytes). Needed to feed write. |
| 3.2 | **SPI 25-series Write Enable + Page Program** | `hwSPI` / protocol layer | 0x06 Write Enable; 0x02 Page Program + 3-byte addr + data (page size e.g. 256 B). Respect WP; optionally use existing WP control. |
| 3.3 | **SPI sector/block erase** | Same layer | Commands 0x20 (4 KB), 0x52, 0xD8; 3-byte address. “Erase” command in CLI could mean “erase whole device” or “erase range”. |
| 3.4 | **CLI: write/flash command** | `main.cpp`, CLIah | e.g. `splasher input.bin -b 16M --write` to flash from file. Reuse `Device` and file manager; call new `splasher::writeFileToFlash()` (or similar). |
| 3.5 | **CLI: erase option** | `main.cpp`, CLIah | e.g. `--erase` (full) or `--erase-range 0,64K`. Map to erase commands above. |

**Exit criteria:** Can flash a binary from file to SPI flash and optionally erase before/after; CLI supports write and erase.

---

## Phase 4: DSPI, QSPI, I2C

**Goal:** Implement additional interfaces per README (DSPI, QSPI, I2C).

| # | Task | Where | Notes |
|---|------|--------|------|
| 4.1 | **DSPI (Dual SPI)** | New `hwDSPI` (or extend `hwSPI`) | Two data lines for read (and often write). Same 25-series commands; different bit clocking. Use protocol abstraction from Phase 2. |
| 4.2 | **QSPI (Quad SPI)** | New class / mode | Four data lines. Again, same command set, different transfer. |
| 4.3 | **I2C (24-series)** | `hwI2C` | Implement bit-banged I2C (or use pigpio I2C if available). Different command set; add 24-series protocol bytes and “read bytes” style API. |
| 4.4 | **Interface selection in CLI** | `main.cpp`, CLIah | e.g. `--spi`, `--dspi`, `--qspi`, `--i2c` (or `--interface spi`). Set `Device.interface` and dispatch to correct implementation in dump/write. |

**Exit criteria:** User can select interface; dump (and ideally write) work for SPI, DSPI, QSPI, and I2C where hardware allows.

---

## Phase 5: Polish & docs

| # | Task | Where | Notes |
|---|------|--------|------|
| 5.1 | **Usage section** | README | Replace “Usage TODO” with examples: dump, write, erase, JEDEC, speed/offset. |
| 5.2 | **CLIah Variable type** | `CLIah.cpp` | Complete “Variable type” handling if needed for `--key=value` style args. |
| 5.3 | **CLIah function pointer on match** | `CLIah.hpp` | Optional: callback when an arg is detected; low priority unless you want extensible hooks. |
| 5.4 | **Remove “EVALUATION DEMO ONLY”** | `main.cpp` | When ready for release. |
| 5.5 | **Call `gpioTerminate()`** | `main.cpp` | Uncomment and call on all exit paths so pigpio is shut down cleanly. |

---

## Dependency overview

```
Phase 1 (SPI read clean) ──► Phase 2 (JEDEC + abstraction) ──► Phase 3 (write/erase)
                                    │
                                    └──► Phase 4 (DSPI, QSPI, I2C)
                                    
Phase 5 (polish) can be done in parallel or last.
```

Suggested order: **1 → 2 → 3 → 4 → 5**, with Phase 5 items picked up anytime.

---

## Quick reference: all TODOs

| Source | Item |
|--------|------|
| README | JEDEC ID |
| README | DSPI |
| README | QSPI |
| README | I2C |
| README | init read / init write |
| README | inherited class members (for expansion) |
| main.cpp | long help “By default .......” |
| main.cpp | move `BinFile*` to main |
| main.cpp | gpio init error message and exit |
| main.cpp | speed default message (remove or move to helper) |
| main.cpp | bytes auto-detect |
| hardware.cpp | Write Protect enable/disable |
| hardware.cpp | Don’t force SPI (interface dispatch) |
| hardware.cpp | inherit “readBytes” style API |
| hardware.cpp | configurable read offset (address) |
| hardware.hpp | namespace for common limits |
| hardware.hpp | pass binary file via pointer (if needed) |
| hardware.hpp | protocol-specific command bytes |
| CLIah | Variable type |
| CLIah | function pointer on match (optional) |
