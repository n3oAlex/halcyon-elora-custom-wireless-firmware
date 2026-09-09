/*
 * Keymap behavior that switches the local display and its backlight off until pressed again.
 *
 * Global locality: the central runs it (no-op there) and forwards it to every peripheral, so the
 * half with the display reacts wherever the key sits. ZMK's own idle handling still blanks on
 * idle and unblanks on activity; the forced-off state re-applies itself after each unblank.
 */

#define DT_DRV_COMPAT zmk_behavior_display_toggle

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_ENABLED(CONFIG_ZMK_DISPLAY) && DT_HAS_CHOSEN(zephyr_display)

#include <zephyr/drivers/display.h>
#include <zephyr/drivers/led.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

static const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

#if DT_HAS_CHOSEN(zmk_display_led)
static const struct device *display_led = DEVICE_DT_GET(DT_PARENT(DT_CHOSEN(zmk_display_led)));
static const uint8_t display_led_idx = DT_NODE_CHILD_IDX(DT_CHOSEN(zmk_display_led));
#endif

static bool forced_off;

static void apply_cb(struct k_work *work) {
    if (forced_off) {
        display_blanking_on(display);
#if DT_HAS_CHOSEN(zmk_display_led)
        led_off(display_led, display_led_idx);
#endif
    } else {
#if DT_HAS_CHOSEN(zmk_display_led)
        led_on(display_led, display_led_idx);
#endif
        display_blanking_off(display);
    }
}

K_WORK_DELAYABLE_DEFINE(apply_work, apply_cb);

static void apply(k_timeout_t delay) {
    if (!zmk_display_is_initialized()) {
        return;
    }
    k_work_reschedule_for_queue(zmk_display_work_q(), &apply_work, delay);
}

/* ZMK queues its unblank from this same event. Listener order is link order, so wait long enough
 * for that unblank to have run, then blank again. */
static int activity_cb(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev != NULL && forced_off && ev->state == ZMK_ACTIVITY_ACTIVE) {
        apply(K_MSEC(300));
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(behavior_display_toggle, activity_cb);
ZMK_SUBSCRIPTION(behavior_display_toggle, zmk_activity_state_changed);

static void toggle(void) {
    forced_off = !forced_off;
    LOG_INF("display forced %s", forced_off ? "off" : "on");
    apply(K_NO_WAIT);
}

#else /* no display on this part */

static void toggle(void) {}

#endif

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    toggle();
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_display_toggle_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_display_toggle_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
