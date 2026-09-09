# PLAN: Halcyon Elora rev2 wireless ZMK config repo
Status: planned

## Requirement
ZMK config repo `n3oAlex/halcyon-elora-custom-wireless-firmware` (public) for a Halcyon Elora rev2,
wireless controllers + dongle, coincell battery boards, left half: Halcyon Rotary Encoder rev2,
right half: Halcyon Cirque Touchpad. Editable at nickcoutsos keymap-editor; every push builds
firmware via GitHub Actions. Keymap = 1:1 port of `example.vil` (Vial/QMK Elora rev1 layout).

## Decisions (grilling session 2026-09-09)
- Battery: `mod_battery_coincell` both halves.
- ZMK Studio on the dongle, `CONFIG_ZMK_STUDIO_LOCKING=n` ("turn off keyboard locking").
- Touchpad cursor 2x via `&zip_xy_scaler 2 1` on the dongle listener; driver touch gain stays "2x".
- Touchpad scrolls while NAV held (listener child node, layers = <1>, `zip_xy_to_scroll_mapper`).
- Encoder: rotate Vol-/Vol+ (BASE), scroll (NAV); press = plain `&kp C_PP` (BASE), `C_MUTE` (NAV).
- Left inner row-3 keys: C_PREV, C_NEXT. Right inner row-3 keys (under touchpad): `&mkp LCLK`, `&mkp RCLK`.
- SYS layer: combo of both outermost thumb-row keys (unused in .vil), `&mo SYS`, timeout 150 ms, slow-release.
- Hold-taps: `&mt`/`&lt` global override: tapping-term 175, flavor balanced, quick-tap 150.
- Layers: 0 BASE, 1 NAV, 2 NUM, 3 CMD, 4 GAME, 5 SYS. Vial layers 5-7 (empty) dropped.
- RGB compiled, off at boot (splitkb default). settings_reset targets for both boards.
- Repo cleaned: other keyboard JSONs + PORTING.md deleted, README rewritten.
- Initial commit + push to main by agent; watch Actions until green.

## Facts that shaped the layout (verified in source)
- Only ONE `config/*.overlay` is applied per build; `halcyon_elora.overlay` would shadow `_left/_right/_dongle`.
  -> use per-target overlays only, never a shared one. `.conf` files all merge.
- keymap-editor needs `config/halcyon_elora.keymap` + `config/halcyon_elora.json` (same basename);
  it rewrites `#include`s and cannot see `.dtsi` files -> keep all behaviors/combos in the keymap,
  hardware tweaks in overlays. No `#ifdef` in keymap.
- Keymap binding order = `halcyon_elora.json` layout order: rows 0-2 (6+6), row 3 (6+2 | 2+6),
  thumbs (5+5, outer->inner | inner->outer), halcyon row (5+5; only left slot 0 = encoder button is real).
- sensors node order: left_halcyon, right_halcyon, left_soldered, right_soldered -> 4 sensor-bindings per layer.
- `&msc` via encoder needs a `zmk,behavior-sensor-rotate` with `tap-ms >= 16`, else nothing is sent.
- Cirque driver here is Zephyr in-tree `cirque,pinnacle`; `sensitivity` 1x = most sensitive.

## .vil -> ZMK translation
| Vial | ZMK |
|---|---|
| LCTL_T(TAB) | `&mt LCTRL TAB` |
| RSFT_T(QUOTE) | `&mt RSHFT SQT` |
| RGUI_T(DOT) / RALT_T(SLASH) / RCTL_T(ENTER) | `&mt RGUI DOT` / `&mt RALT FSLH` / `&mt RCTRL RET` |
| MEH_T(TAB) | `&mt LS(LC(LALT)) TAB` |
| ALL_T(BSPACE) | `&mt LS(LC(LA(LGUI))) BSPC` |
| LGUI_T(ENTER) / RALT_T(DELETE) | `&mt LGUI RET` / `&mt RALT DEL` |
| LT1(SPACE) | `&lt NAV SPACE` |
| MO(2) | `&mo NUM` |
| HYPR(x) | `&kp LS(LC(LA(LG(x))))` |
| 0xc50 / 0xc4f | `&kp LA(LG(LEFT))` / `&kp LA(LG(RIGHT))` |
| TD(0) tap Cmd+X, hold MO(3), dbl Cmd+C | hold-tap `&ht_cmd CMD 0` -> hold `&mo`, tap `&td_cut_copy` (tap-dance LG(X), LG(C)) |
| TD(1) tap Ctrl+B, hold RSHIFT | `&mt RSHFT LC(B)` |
| TD(3) play / dbl mute | plain `&kp C_PP` on encoder; mute on NAV+encoder |
| combos VOLU+MPLY->MPRV, VOLD+MPLY->MNXT | replaced by left inner keys |
| other 8 combos | ZMK combos by position (LALT+SPACE->`&mo NUM`, ESC+MINUS->`&tog GAME`, D+F `}`, J+K `]`, C+V `LA(LBKT)`, M+, `LA(RBKT)`, E+R `LA(N9)`, U+I `LA(N0)`) |
| GAME inner-column keys L/Z/M (left), LALT/LGUI/RALT (right) | only 2 slots/side: left Z, M; right LALT, LGUI. L and RALT dropped (flagged) |

## Files
- `build.yaml` — dongle (`halcyon_elora_dongle mod_cirque_central`, Studio + snippet),
  left (`halcyon_elora_left mod_battery_coincell mod_encoder_left`, `-DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n`),
  right (`halcyon_elora_right mod_battery_coincell mod_cirque_hw_right`), settings_reset x2.
- `config/west.yml` — unchanged from template.
- `config/halcyon_elora.keymap` — layers, behaviors, combos, sensor-bindings.
- `config/halcyon_elora.json` — keep (template).
- `config/halcyon_elora_dongle.conf` — `CONFIG_ZMK_STUDIO_LOCKING=n`.
- `config/halcyon_elora_dongle.overlay` — trackpad listener processors (scaler, NAV scroll).
- `README.md` — rewritten. Delete `PORTING.md`, `config/halcyon_{corne,ferris,kyria,lily58}.json`.
- `.github/workflows/build.yml` — unchanged. `.gitignore` — `.agents/tmp/`, `.agents/archive/*`.

## TODO
- [ ] scaffold repo from template (copy, clean)
- [ ] write build.yaml, conf, overlay
- [ ] write keymap (port), self-check binding counts (72 per layer, 4 sensor bindings)
- [ ] README
- [ ] git init, commit, `gh repo create --public --source . --push`
- [ ] watch Actions, fix until green
- [ ] user: install keymap-editor GitHub App, select repo; flash 3 uf2

## Open / flagged
- GAME layer loses L (left) and RALT (right) inner keys; user to reassign in editor.
- Scroll speed/direction on the touchpad and encoder are untestable without hardware; knobs documented in README.
- Host assumed macOS (Cmd shortcuts, natural scroll).
