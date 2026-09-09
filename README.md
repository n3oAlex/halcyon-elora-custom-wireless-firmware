# Halcyon Elora custom wireless firmware

ZMK config for one specific keyboard:

| Part | Choice |
| :--- | :--- |
| Keyboard | Halcyon Elora rev2 |
| Controllers | Halcyon Wireless, both halves, plus Halcyon Dongle |
| Battery boards | Coincell (`mod_battery_coincell`) |
| Left half module | None yet; e-paper display on the way (`halcyon_elora_left_epaper`). See "Modules and the wireless controllers" |
| Right half module | Halcyon Cirque Touchpad (`mod_cirque_hw_right`) |
| Host | macOS, dongle on USB |

Built on [splitkb/zmk-halcyon-config](https://github.com/splitkb/zmk-halcyon-config) and
[splitkb/zmk-halcyon-module](https://github.com/splitkb/zmk-halcyon-module). Every push builds
firmware with GitHub Actions.

## Firmware artifacts

Each Actions run produces one zip per target. Flash the three keyboard images in this order:
dongle first, then left, then right. Enter the bootloader by double-tapping the reset button
(or from the Sys layer), copy the `.uf2` onto the `HALCYON` drive that appears, and wait for the
drive to disconnect on its own.

**After flashing a wireless half, press its reset button once.** The Adafruit nRF52 bootloader
leaves a peripheral enabled after a UF2 flash, so the controller draws roughly ten times its
normal idle current until it is reset
([adafruit/Adafruit_nRF52_Bootloader#368](https://github.com/adafruit/Adafruit_nRF52_Bootloader/issues/368),
and the [splitkb firmware guide](https://docs.splitkb.com/product-guides/halcyon-series/build-guide/wireless/firmware)
says the same). Flashing alone does not reset the chip. The dongle is USB powered, so it does
not matter there, but resetting it does no harm.

On macOS Sonoma and later, Finder reports **error -36** at the end of every UF2 copy. Ignore it.
The controller resets itself the moment the last block arrives, before Finder gets its
acknowledgement, so the flash is already done ([ZMK: flashing issues](https://zmk.dev/docs/troubleshooting/flashing-issues)).
A flash that really failed leaves the `HALCYON` drive mounted, or the part comes back into the
bootloader by itself.

The halves send nothing to the computer over USB; the dongle is the only HID device. Plugging
a half in only charges it and, with a debug build, exposes its log.

| Artifact | Flash to |
| :--- | :--- |
| `halcyon_elora_dongle` | Dongle. ZMK Studio enabled, no unlock needed. |
| `halcyon_elora_left` | Left half, no module. |
| `halcyon_elora_left_epaper` | Left half with the e-paper display (mountain image). |
| `halcyon_elora_right` | Right half (touchpad). |
| `settings_reset_controller` | A half that refuses to pair. Flash, wait 5 s, then flash its normal image again. |
| `settings_reset_dongle` | Same for the dongle. |
| `debug_*` | Same firmware plus USB logging. Diagnosis only, see below. |

After a settings reset on any part, reset all three so they re-pair from scratch.

## Debugging modules

The `debug_halcyon_elora_left`, `debug_halcyon_elora_right` and `debug_halcyon_elora_dongle`
artifacts are the normal builds with USB logging at debug level for ZMK, the sensor drivers
(a rev 2 encoder) and the input drivers (touchpad). They cost battery and the dongle debug build has no
ZMK Studio, so flash the normal images again afterwards.

1. Flash the debug image onto the part you want to inspect.
2. Run the capture tool from the repo root. It walks you through unplugging and plugging the
   part so it finds the right serial port, then records to `logs/<part>-<time>.log`:

```sh
python3 tools/capture-log.py left      # or right, or dongle
```

3. Follow the on-screen sequence: press reset once (boot lines), wait for it to reconnect, type a
   few keys, then use the module (turn and press the encoder, or tap and swipe the touchpad).
4. Ctrl-C ends the capture and prints a summary. The file stays in `logs/` (gitignored).

Only Python 3 is needed. If the automatic detection picks the wrong port, pass `--device`; list
ports with `--list`.

What to look for:

- Left half: boot lines, then `split` once it connects and a position event per key. With a rev 2
  encoder, `EC11` lines when it turns and position 62 when its button is pressed.
- Right half: `pinnacle` at boot (driver init, any error code) and `input` events on touch.
- Dongle: `split` connection lines for both peripherals, then `sensor` or `input` events arriving
  from them.

### Check these before reading logs

- **VIK voltage selector switch on the keyboard PCB.** splitkb's Cirque guide requires it on
  **NC** for the wireless build; the encoder guide allows NC or 5V. On 5V the module rail only
  exists while USB is plugged in, so a touchpad that works on a cable and dies on battery is the
  textbook symptom of a right half left on 5V. Move it with a plastic tool, not metal.
- **Left VIK connector.** Two modules on the left half behaved like missing contacts (a rev 1.0
  encoder whose lines never moved, a TFT whose backlight lit but whose panel stayed blank, both on
  USB too). A damaged board-side contact is the suspicion; the e-paper on the same connector will
  tell. Swapping in the right half's known-good cable is the cheapest test.
- **Flat cables.** Insert with the blue side up, push fully home, lock the tab. Both modules hang
  off the same VIK connector on their half.
Comparing against the stock firmware for the same hardware from
[splitkb.com/fw](https://splitkb.com/fw) tells whether the fault is in this config or elsewhere:
this repo uses the identical shield lists, module and ZMK revisions as that generator.

## Editing the keymap

Open <https://nickcoutsos.github.io/keymap-editor/>, log in with GitHub, install the
keymap-editor GitHub App on this repository, and pick `halcyon_elora.keymap`. Saving commits to
the selected branch, which triggers a build. The editor sees everything in
`config/halcyon_elora.keymap`: layers, hold-taps, tap-dances, combos, encoder bindings. Keep
hardware tweaks out of that file; the editor rewrites `#include` lines and does not read other
files.

ZMK Studio (over USB on the dongle) can also change the keymap on the fly. Studio edits are
stored in flash and override the compiled keymap until you choose "Restore stock settings" in
Studio, so treat Studio as a scratchpad and this repo as the source of truth.

## Layers

Ported 1:1 from a Vial/QMK Elora rev1 layout.

| # | Name | Reach it with |
| :--- | :--- | :--- |
| 0 | Base | |
| 1 | Nav | hold right thumb Space |
| 2 | Num | hold either thumb NUM, or combo left thumb Alt + right thumb Space |
| 3 | Cmd | on Nav, hold the cut/copy key (`;` position) |
| 4 | Game | toggle with combo Esc + Minus |
| 5 | Sys | hold both outermost thumb-row keys |

Rev2-specific keys:

- Encoder (only with a rev 2 encoder module): volume; press is play/pause. On Nav: scroll; press is mute.
- Left inner row-3 keys: previous / next track.
- Right inner row-3 keys (under the touchpad): left click / right click. Tap-to-click is also on.
- Touchpad modes: normal cursor at 3x, scroll while Space (Nav) is held, precision (raw 1:1)
  while the NUM thumb key is held. Scrolling cannot share the Hyper key: macOS reads Cmd+wheel
  as zoom and Shift+wheel as horizontal scroll.
- The Game layer had three inner-column keys per side on rev1 and two on rev2. `L` (left) and
  `RALT` (right) were dropped; reassign them in the editor if you use them.

## Modules and the wireless controllers

splitkb's [Halcyon Wireless announcement](https://blog.splitkb.com/introducing-halcyon-wireless/)
lists what works with these controllers: the Cirque touchpad (power hungry, LiPo recommended),
the rotary encoder **rev 2** (rev 1.0 is hardware-incompatible), and the e-paper display. The
TFT display builds but drains a battery within an hour, so it is not offered here.

`halcyon_elora_left_epaper` builds splitkb's `mod_display_epaper_mountain` shield (the other
images are `forest` and `cityscape`, swap the shield name in `build.yaml`). The left half is a
split peripheral, so the display shows what the peripheral status screen offers: its own battery,
whether it is connected, and the image. Layer, lock and right-half battery state live on the
central (the dongle), and neither ZMK nor splitkb's fork sends any of it back to a peripheral:
the only central-to-peripheral messages are behavior invocations, the physical layout index and
HID lock indicators. Showing dongle-side status on that screen would mean custom split code on
both ends. E-paper only draws power while refreshing, so no idle blanking is configured; raise
`CONFIG_ZMK_IDLE_TIMEOUT` if it refreshes more often than you like.

For a rev 2 encoder module, add `mod_encoder_left` to the left shield list in `build.yaml`; the
keymap's encoder bindings and the halcyon-row play/pause key are already there.

## Coincell battery board and modules

On the coincell board the VIK module rail is behind ZMK's "external power" switch, and splitkb's
coincell shield (since 2026-09-08) refuses to enable external power while USB is disconnected.
Result: modules work on a cable and die on battery. `config/halcyon_elora_left.conf` and
`_right.conf` override that and start with external power on. splitkb wired the orange LED into
the same switch as an indicator, so it would burn continuously; `config/coincell_ext_power.dtsi`
(included by both half overlays) takes the LED back out and drives only the rail. With the LiPo
board, switch `build.yaml` to `mod_battery_lipo` and drop the two `CONFIG_ZMK_EXT_POWER*` lines
plus the two half overlays.

## Where each tweak lives

| Want to change | File |
| :--- | :--- |
| Keys, layers, combos, hold-tap timing (175 ms, balanced, quick-tap 150) | `config/halcyon_elora.keymap` |
| Touchpad speed (`&zip_xy_scaler 3 1`), precision and scroll modes | `config/halcyon_elora_dongle.overlay` |
| Studio locking (off), other dongle-only Kconfig | `config/halcyon_elora_dongle.conf` |
| Kconfig shared by all three builds | `config/halcyon_elora.conf` |
| Build targets and modules | `build.yaml` |
| Orange LED off while external power stays on (coincell) | `config/coincell_ext_power.dtsi` |
| Physical layout for the keymap editor | `config/halcyon_elora.json` |

Rules that are easy to trip over:

- ZMK applies exactly one overlay from `config/` per build, picked by shield name. A shared
  `halcyon_elora.overlay` would shadow the dongle one. Use per-target overlays only
  (`halcyon_elora_dongle`, `halcyon_elora_left`, `halcyon_elora_right`); every left target shares
  the left one.
- `.conf` files are merged, so `halcyon_elora.conf` plus `halcyon_elora_<target>.conf` both apply.
- Encoder scroll amount is `CONFIG_ZMK_POINTING_DEFAULT_SCRL_VAL` (set it in the dongle conf).
- Touch detection gain is the driver's `sensitivity` property (`1x` most sensitive, splitkb
  ships `2x`). To change it, add `config/halcyon_elora_right.overlay` with
  `&trackpad { sensitivity = "1x"; };`.

## Untested knobs

Scroll speed and direction for the touchpad were set without hardware, and the e-paper build has
not been flashed yet. If scrolling feels wrong, the comments in
`config/halcyon_elora_dongle.overlay` and the Sys/Nav notes above point at the single value to
change.

## Local builds

Follow the [ZMK local toolchain guide](https://zmk.dev/docs/development/local-toolchain/setup)
and add `splitkb/zmk-halcyon-module` as a module; `config/west.yml` already lists it.
