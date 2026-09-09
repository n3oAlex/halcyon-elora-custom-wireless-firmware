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
(or from the Sys layer) and copy the `.uf2` onto the USB drive that appears.

| Artifact | Flash to |
| :--- | :--- |
| `halcyon_elora_dongle` | Dongle. ZMK Studio enabled, no unlock needed. |
| `halcyon_elora_left` | Left half (encoder). |
| `halcyon_elora_right` | Right half (touchpad). |
| `settings_reset_controller` | A half that refuses to pair. Flash, wait 5 s, then flash its normal image again. |
| `settings_reset_dongle` | Same for the dongle. |

After a settings reset on any part, reset all three so they re-pair from scratch.

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
- The Game layer had three inner-column keys per side on rev1 and two on rev2. `L` (left) and
  `RALT` (right) were dropped; reassign them in the editor if you use them.

## Where each tweak lives

| Want to change | File |
| :--- | :--- |
| Keys, layers, combos, hold-tap timing (175 ms, balanced, quick-tap 150) | `config/halcyon_elora.keymap` |
| Touchpad cursor speed (`&zip_xy_scaler 2 1`) and scroll-on-Nav | `config/halcyon_elora_dongle.overlay` |
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
