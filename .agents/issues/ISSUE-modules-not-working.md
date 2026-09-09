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

## Fourth report (user, 2026-09-09)
- Today's stock firmware: left keys work (cable or not), encoder dead entirely; right half dead entirely.
  Worse than our build, where right keys worked. Points at splitkb's 09-08 commits, or a flat right coincell.
- Added tools/capture-log.py (stdlib) to replace screen; README updated.
- Local branch `pre-0908` pins module 66ec164 + fork f6797e3 (parent of the two 09-08 commits). Not pushed.

## Log findings (2026-09-09 21:03-21:05, debug builds)
- LEFT half (USB): EC11 inits on P0.05/P0.04, kscan direct on P0.25 (encoder button). Split link up,
  dongle subscribed to position + sensor CCC. Encoder produced exactly TWO edges (A then B, delta -1
  each, both forwarded via split_peripheral_listener) then nothing more while the user kept turning.
  No encoder-button (position 62) event at all. Battery ADC: 987 mV -> 0% (reading may be
  meaningless while on USB; do not trust yet).
- RIGHT half: only 0.4 s of boot captured; tool then latched onto the dongle port (bug, fixed:
  reconnect now only adopts a NEWLY appeared port).
- DONGLE (unintended capture): right half connects over BLE, input characteristic found and
  subscribed, touchpad input events arrive, listener scales them (3/1) and sets HID mouse movement.
  No USB send errors. Also spams "Failed to untrack released key -19" / "Tried to release button 0
  too often" on every touch report: pinnacle primary-tap reports INPUT_BTN_TOUCH releases the
  listener maps to mouse button 0. Noise, not blocking.
- Conclusion so far: when the right half is on USB the whole chain works up to the HID report. Need
  the dongle log with the halves on BATTERY (capture running, logs/dongle-*.log) to see whether input
  and sensor events still arrive.

## Dongle capture with BOTH halves on battery (logs/dongle-20260909-210852.log)
- Left connects, position + sensor CCC subscribed; key presses arrive (0, 15, 16, 27, 28).
  ZERO sensor notifications during full encoder turns; ZERO position-62 (encoder button) events.
- Right connects, input characteristic subscribed; key presses arrive (31, 32, 33).
  ZERO input events during touchpad use.
- Earlier, with the right half on USB, hundreds of input events reached the dongle and became HID
  mouse movement. With the left half on USB, the encoder produced 2 edges.
=> modules dead on battery on BOTH halves, alive on USB. Common cause = module power on battery.
Test: config/halcyon_elora_{left,right}.conf override EXT_POWER_DISABLE_WHEN_USB_DISCONNECTED=n +
EXT_POWER_START=y. If modules work on battery afterwards: VIK rail depends on EXT_POWER and the
09-08 coincell change broke modules for coincell users -> report to splitkb, keep override (maybe
drop the orange LED from EXT_POWER control-gpios in our overlay to save battery).
If not: voltage/hardware on battery (Pinnacle undervolt would not explain a passive encoder, though).

## RESOLVED (touchpad) 2026-09-09: EXT_POWER override -> touchpad works on battery. Orange LEDs on.
## Encoder: module is rev 1.0 (ALPS EC12, 2 edges/click). QMK userspace hlc_encoder/config.h: A=GP27
(AD_1), B=GP26 (AD_2), button GP16 (SDA); rev2 only changes ENCODER_RESOLUTION 2->4. Same pins as
ZMK mod_encoder (A/B swapped = direction only). Fork EC11 one-active-pin driver stops after first
click. Fix: vendored upstream two-line driver as halcyon,ec11-classic (drivers/, dts/, module.yml
cmake+kconfig), left overlay re-types node, steps=40, left conf disables fork EC11.

## Fifth report (user, 2026-09-09 22:02): encoder still dead, wired or not, with ec11-classic
- logs/left-20260909-220212.log: identical signature to the fork driver log. Init reads A=0,B=0
  (both contacts closed = EC12 closed detent). One A rising edge, 53 ms later one B rising edge
  (state 10 -> 11), then nothing for the rest of the capture. Interrupts re-armed fine after each.
- Two different drivers (fork one-active-pin, upstream two-line) fail identically => not driver
  logic. Either the lines physically never go low again (module/cable/connector/encoder) or nRF
  edge interrupts stop after the first two events (falling edges never seen).
- Fork rewrite (69725d4) commit message: "fix missing pulses" (reads both pins on any edge). A real
  improvement, not the bug.
- Encoder button (vik_conn 0, kscan-gpio-direct, position 62) still never seen in any log.
- Test: CONFIG_EC11_CLASSIC_DEBUG_POLL (debug left build only) logs raw A/B levels every 50 ms
  on change + 2 s heartbeat. Lines change while turning but no EC11C interrupt lines => firmware.
  Lines stuck at 1/1 while turning => hardware (encoder common not reaching GND after first click,
  cable/connector).

## Poll result (user, 2026-09-09 22:26, logs/left-20260909-222645.log): HARDWARE
- 29 poll lines, every one A=1 B=1, no "(changed)", no EC11C interrupt lines, keys 36/39 fine,
  no position 62. Init read 11 this boot (earlier boots read 00 then one half-click: a flaky
  common/ground contact fits). Lines never pulled low => encoder common not reaching GND:
  module, flat cable or VIK connector. Stock firmware showed the same. No firmware path forward.
- User decision: install the Display module rev 1.0 on the left instead. Assumed = Halcyon TFT
  LCD Display Module (mod_display_tft, ST7789 135x240); e-paper target built as fallback.
- Repo change: left target -> mod_display_tft; rev 1.0 encoder support moved into add-on shield
  boards/shields/mod_encoder_rev1_left (overlay + conf) with its own targets
  halcyon_elora_left_encoder / debug_halcyon_elora_left_encoder. config/halcyon_elora_left.overlay
  removed (its only content moved). Open: display on peripheral untested; coincell drain with TFT.

## Sixth report (user, 2026-09-09): TFT dead on coincell, orange LEDs
- TFT: backlight dim/brief per key press, nothing drawn, left half delivers no keys. Reads as
  CR2032 brownout under TFT load (reboot loop). Not verified (no log, no USB test). User chose to
  drop the left module: halcyon_elora_left = no module; _tft/_epaper/_encoder kept as alternates
  for the LiPo board. dispoff behavior stays (no-op without display).
- Orange LEDs: coincell overlay puts P0.31 (orange LED) into EXT_POWER control-gpios next to the
  rail pin halcyon_conn 32. Fix: config/coincell_ext_power.dtsi overrides control-gpios to the
  rail only, included from halcyon_elora_left.overlay and _right.overlay.
