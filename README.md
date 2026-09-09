# Halcyon Elora custom wireless firmware

ZMK config for one specific keyboard:

| Part | Choice |
| :--- | :--- |
| Keyboard | Halcyon Elora rev2 |
| Controllers | Halcyon Wireless, both halves, plus Halcyon Dongle |
| Battery boards | Coincell (`mod_battery_coincell`) |
| Left half module | Halcyon Rotary Encoder rev2 (`mod_encoder_left`) |
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

The halves send nothing to the computer over USB; the dongle is the only HID device. Plugging
a half in only charges it and, with a debug build, exposes its log.

| Artifact | Flash to |
| :--- | :--- |
| `halcyon_elora_dongle` | Dongle. ZMK Studio enabled, no unlock needed. |
| `halcyon_elora_left` | Left half (encoder). |
| `halcyon_elora_right` | Right half (touchpad). |
| `settings_reset_controller` | A half that refuses to pair. Flash, wait 5 s, then flash its normal image again. |
| `settings_reset_dongle` | Same for the dongle. |
| `debug_*` | Same firmware plus USB logging. Diagnosis only, see below. |

After a settings reset on any part, reset all three so they re-pair from scratch.

## Debugging modules (encoder, touchpad)

The `debug_halcyon_elora_left`, `debug_halcyon_elora_right` and `debug_halcyon_elora_dongle`
artifacts are the normal builds with USB logging at debug level for ZMK, the sensor drivers
(encoder) and the input drivers (touchpad). They cost battery and the dongle debug build has no
ZMK Studio, so flash the normal images again afterwards.

1. Flash the debug image onto the part you want to inspect and press reset.
2. Keep it connected over USB. It shows up as a serial device.
3. Read the log and reproduce the problem (turn the encoder, press its button, touch the pad):

```sh
ls /dev/tty.usbmodem*
screen /dev/tty.usbmodemXXXX 115200      # quit with ctrl-a then k
```

What to look for:

- Left half: lines mentioning `ec11` or `sensor` when the encoder turns, and a `kscan` /
  position event for position 62 when its button is pressed.
- Right half: `pinnacle` at boot (driver init, any error code) and `input` events on touch.
- Dongle: `split` connection lines for both peripherals, then `sensor` or `input` events arriving
  from them.

### Check these before reading logs

- **VIK voltage selector switch on the keyboard PCB.** splitkb's Cirque guide requires it on
  **NC** for the wireless build; the encoder guide allows NC or 5V. On 5V the module rail only
  exists while USB is plugged in, so a touchpad that works on a cable and dies on battery is the
  textbook symptom of a right half left on 5V. Move it with a plastic tool, not metal.
- **Encoder that sends exactly one step and then nothing.** splitkb's ZMK fork replaced the EC11
  driver with one that arms an interrupt on only one of the two encoder lines at a time and swaps
  after each edge. If the second line (B, VIK pin AD_1) never reaches the controller, the driver
  emits one event on the first A edge and then waits forever. That points at the encoder module's
  flat cable not being fully seated, or a bad line, not at the config. The debug log shows it as a
  first `EC11` state line with nothing after it, or `Unable to set B pin GPIO interrupt`.
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

- Encoder: volume; press is play/pause. On Nav: scroll; press is mute.
- Left inner row-3 keys: previous / next track.
- Right inner row-3 keys (under the touchpad): left click / right click. Tap-to-click is also on.
- Touchpad modes: normal cursor at 3x, scroll while Space (Nav) is held, precision (raw 1:1)
  while the NUM thumb key is held. Scrolling cannot share the Hyper key: macOS reads Cmd+wheel
  as zoom and Shift+wheel as horizontal scroll.
- The Game layer had three inner-column keys per side on rev1 and two on rev2. `L` (left) and
  `RALT` (right) were dropped; reassign them in the editor if you use them.

## Where each tweak lives

| Want to change | File |
| :--- | :--- |
| Keys, layers, combos, hold-tap timing (175 ms, balanced, quick-tap 150) | `config/halcyon_elora.keymap` |
| Touchpad speed (`&zip_xy_scaler 3 1`), precision and scroll modes | `config/halcyon_elora_dongle.overlay` |
| Studio locking (off), other dongle-only Kconfig | `config/halcyon_elora_dongle.conf` |
| Kconfig shared by all three builds | `config/halcyon_elora.conf` |
| Build targets and modules | `build.yaml` |
| Physical layout for the keymap editor | `config/halcyon_elora.json` |

Rules that are easy to trip over:

- ZMK applies exactly one overlay from `config/` per build, picked by shield name. A shared
  `halcyon_elora.overlay` would shadow the dongle one. Use per-target overlays only
  (`halcyon_elora_dongle`, `halcyon_elora_left`, `halcyon_elora_right`).
- `.conf` files are merged, so `halcyon_elora.conf` plus `halcyon_elora_<target>.conf` both apply.
- Encoder scroll amount is `CONFIG_ZMK_POINTING_DEFAULT_SCRL_VAL` (set it in the dongle conf).
- Touch detection gain is the driver's `sensitivity` property (`1x` most sensitive, splitkb
  ships `2x`). To change it, add `config/halcyon_elora_right.overlay` with
  `&trackpad { sensitivity = "1x"; };`.

## Untested knobs

Scroll speed and direction for the touchpad and the encoder were set without hardware. If they
feel wrong, the comments in `config/halcyon_elora_dongle.overlay` and the Sys/Nav notes above
point at the single value to change.

## Local builds

Follow the [ZMK local toolchain guide](https://zmk.dev/docs/development/local-toolchain/setup)
and add `splitkb/zmk-halcyon-module` as a module; `config/west.yml` already lists it.
