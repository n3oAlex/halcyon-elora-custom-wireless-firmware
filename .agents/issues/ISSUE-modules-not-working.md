# ISSUE: encoder and Cirque touchpad do nothing on first hardware test
Status: in-progress

## Report (user, 2026-09-09)
Halves work through the dongle after several resets. Encoder (left) and Cirque (right) do nothing.
Unknown: whether the encoder push button works, whether reset was pressed after flashing,
whether the stock splitkb firmware behaves the same.

## Verified from the CI logs of run 34362134617
- Left build: CONFIG_EC11=y, mod_encoder_left.overlay + .conf applied, SPLITKB_VIK_SLEEP=y.
- Right build: CONFIG_INPUT_PINNACLE=y, CONFIG_ZMK_INPUT_SPLIT=y, mod_cirque_hw_right.overlay applied.
- Dongle: ZMK_INPUT_SPLIT=y, ZMK_POINTING=y, trackpad_listener status okay with device trackpad_split,
  PERIPHERALS=2, sensors node lists all four encoders.
- Shield lists and flags equal splitkb's generate-build-yaml.py output for this hardware
  (halcyon_elora_dongle / _left_cc_encoder / _right_cc_cirque).
- Module revision at build time includes 2054c04 (2026-09-08, coincell: EXT_POWER off when USB
  disconnected). EXT_POWER = halcyon_conn 32 = RGB rail; VIK 3V3 = halcyon_conn 33, driven high at
  boot by vik_sleep_init (POST_KERNEL). So that commit should not unpower the modules, on paper.

## Not the cause
- Bootloader issue #368: only raises idle current until reset; does not affect function.

## New data (user, 2026-09-09, second report)
- Touchpad works while the right half is on a USB cable; dies on battery. Right-half keys keep working.
- Encoder: 2/10 cable reconnects give exactly one volume step, then nothing.

## Third report (user, 2026-09-09)
- Right half VIK selector confirmed on NC. Stock firmware from splitkb.com/fw worked with the Cirque
  wirelessly (date of that stock download unknown: before or after module commit 2054c04?).
- Encoder ribbon cable replaced with a new longer one: no change.
- First debug-build attempt failed: CONFIG_ZMK_USB_LOGGING alone lacks the CDC-ACM DT node; switched to
  the zmk-usb-logging snippet.

## Hypotheses, in order (revised)
0. Since stock worked wirelessly and hardware checks are done: fault is in this config OR in the
   2026-09-08 module/fork commits that today's stock also carries. Bisect: re-download stock today
   and test; then if stock still works, diff is our keymap/overlay/conf only.
1. (was #1) Right half VIK voltage selector on 5V instead of NC. RULED OUT by user. splitkb Cirque wireless guide: "Set the VIK
   voltage selector to NC". Encoder guide: "can be on either 5V or NC". 5V = VBUS only, so the module
   rail exists only on cable. Matches the touchpad symptom exactly. User can check without tools.
2. Encoder line B (vik_conn 3 = halcyon_conn 18 = P0.04, "AD_1") not reaching the MCU. Fork commit
   69725d4 rewrote EC11 to arm ONE pin at a time and swap after each edge; a dead B line yields exactly
   one event then silence. Old two-pin driver would have shown nothing at all. Check FFC seating on
   the left; debug log would show the first EC11 state line and then nothing, or the
   "Unable to set B pin GPIO interrupt" warning.
3. Coincell undervoltage of the Pinnacle (if selector is already NC). Bisect by powering the right
   half from USB with the selector on NC: if it then works on battery too after a reboot, it was 1.
4. Something in the 2026-09-08 commits (module 2054c04, fork 519d336/e22ab82). EXT_POWER on coincell
   = orange LED + RGB rail only (verified in the coincell overlay), VIK rail is separate. Unlikely.

## Not the cause
- Keymap size: 7 layers, 9 combos, 3 custom behaviors is tiny for ZMK on nRF52840; builds fit with
  ample flash. Not related to any symptom.
- Bootloader issue #368: only raises idle current until reset.

## Next steps
- [x] README: post-flash reset, debug procedure, selector + encoder failure signature.
- [x] build.yaml: debug_* targets with USB logging + DBG levels for zmk/sensor/input.
- [x] keymap/overlay: 3x cursor, precision on NAV, scroll layer on Backspace (1/3 speed).
- [ ] user: check right-half VIK selector = NC, reseat both FFCs, retest on battery.
- [ ] user: commit+push, flash debug images, capture logs (left, right, dongle).
- [ ] if selector was already NC and log shows pinnacle errors on battery only: hypothesis 3.
