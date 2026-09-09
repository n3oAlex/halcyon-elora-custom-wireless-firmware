/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT halcyon_ec11_classic

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/logging/log.h>

#include "ec11_classic.h"

#define FULL_ROTATION 360

LOG_MODULE_REGISTER(EC11C, CONFIG_SENSOR_LOG_LEVEL);

static int ec11c_get_ab_state(const struct device *dev) {
    const struct ec11c_config *drv_cfg = dev->config;

    return (gpio_pin_get_dt(&drv_cfg->a) << 1) | gpio_pin_get_dt(&drv_cfg->b);
}

static int ec11c_sample_fetch(const struct device *dev, enum sensor_channel chan) {
    struct ec11c_data *drv_data = dev->data;
    const struct ec11c_config *drv_cfg = dev->config;
    uint8_t val;
    int8_t delta;

    __ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL || chan == SENSOR_CHAN_ROTATION);

    val = ec11c_get_ab_state(dev);

    LOG_DBG("prev: %d, new: %d", drv_data->ab_state, val);

    switch (val | (drv_data->ab_state << 2)) {
    case 0b0010:
    case 0b0100:
    case 0b1101:
    case 0b1011:
        delta = -1;
        break;
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000:
        delta = 1;
        break;
    default:
        delta = 0;
        break;
    }

    LOG_DBG("Delta: %d", delta);

    drv_data->pulses += delta;
    drv_data->ab_state = val;

    // TODO: Temporary code for backwards compatibility to support
    // the sensor channel rotation reporting *ticks* instead of delta of degrees.
    // REMOVE ME
    if (drv_cfg->steps == 0) {
        drv_data->ticks = drv_data->pulses / drv_cfg->resolution;
        drv_data->delta = delta;
        drv_data->pulses %= drv_cfg->resolution;
    }

    return 0;
}

static int ec11c_channel_get(const struct device *dev, enum sensor_channel chan,
                            struct sensor_value *val) {
    struct ec11c_data *drv_data = dev->data;
    const struct ec11c_config *drv_cfg = dev->config;
    int32_t pulses = drv_data->pulses;

    if (chan != SENSOR_CHAN_ROTATION) {
        return -ENOTSUP;
    }

    drv_data->pulses = 0;

    if (drv_cfg->steps > 0) {
        val->val1 = (pulses * FULL_ROTATION) / drv_cfg->steps;
        val->val2 = (pulses * FULL_ROTATION) % drv_cfg->steps;
        if (val->val2 != 0) {
            val->val2 *= 1000000;
            val->val2 /= drv_cfg->steps;
        }
    } else {
        val->val1 = drv_data->ticks;
        val->val2 = drv_data->delta;
    }

    return 0;
}

static const struct sensor_driver_api ec11c_driver_api = {
#ifdef CONFIG_EC11_CLASSIC_TRIGGER
    .trigger_set = ec11c_trigger_set,
#endif
    .sample_fetch = ec11c_sample_fetch,
    .channel_get = ec11c_channel_get,
};

#ifdef CONFIG_EC11_CLASSIC_DEBUG_POLL
#define EC11C_POLL_INTERVAL K_MSEC(50)
#define EC11C_POLL_HEARTBEAT 40 /* polls between unchanged-level log lines (2 s) */

static void ec11c_poll_work_cb(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct ec11c_data *drv_data = CONTAINER_OF(dwork, struct ec11c_data, poll_work);
    const struct ec11c_config *drv_cfg = drv_data->poll_dev->config;
    int a = gpio_pin_get_dt(&drv_cfg->a);
    int b = gpio_pin_get_dt(&drv_cfg->b);
    uint8_t state = (a << 1) | b;
    bool changed = state != drv_data->poll_state;

    if (changed || ++drv_data->poll_count >= EC11C_POLL_HEARTBEAT) {
        LOG_INF("poll A=%d B=%d%s", a, b, changed ? " (changed)" : "");
        drv_data->poll_state = state;
        drv_data->poll_count = 0;
    }

    k_work_schedule(dwork, EC11C_POLL_INTERVAL);
}
#endif

int ec11c_init(const struct device *dev) {
    struct ec11c_data *drv_data = dev->data;
    const struct ec11c_config *drv_cfg = dev->config;

    LOG_DBG("A: %s %d B: %s %d resolution %d", drv_cfg->a.port->name, drv_cfg->a.pin,
            drv_cfg->b.port->name, drv_cfg->b.pin, drv_cfg->resolution);

    if (!device_is_ready(drv_cfg->a.port)) {
        LOG_ERR("A GPIO device is not ready");
        return -EINVAL;
    }

    if (!device_is_ready(drv_cfg->b.port)) {
        LOG_ERR("B GPIO device is not ready");
        return -EINVAL;
    }

    if (gpio_pin_configure_dt(&drv_cfg->a, GPIO_INPUT)) {
        LOG_DBG("Failed to configure A pin");
        return -EIO;
    }

    if (gpio_pin_configure_dt(&drv_cfg->b, GPIO_INPUT)) {
        LOG_DBG("Failed to configure B pin");
        return -EIO;
    }

#ifdef CONFIG_EC11_CLASSIC_TRIGGER
    if (ec11c_init_interrupt(dev) < 0) {
        LOG_DBG("Failed to initialize interrupt!");
        return -EIO;
    }
#endif

    drv_data->ab_state = ec11c_get_ab_state(dev);

#ifdef CONFIG_EC11_CLASSIC_DEBUG_POLL
    drv_data->poll_dev = dev;
    drv_data->poll_state = drv_data->ab_state;
    drv_data->poll_count = 0;
    k_work_init_delayable(&drv_data->poll_work, ec11c_poll_work_cb);
    k_work_schedule(&drv_data->poll_work, EC11C_POLL_INTERVAL);
#endif

    return 0;
}

#define EC11_CLASSIC_INST(n)                                                                               \
    static struct ec11c_data ec11c_data_##n;                                                         \
    static const struct ec11c_config ec11c_cfg_##n = {                                               \
        .a = GPIO_DT_SPEC_INST_GET(n, a_gpios),                                                    \
        .b = GPIO_DT_SPEC_INST_GET(n, b_gpios),                                                    \
        .resolution = DT_INST_PROP_OR(n, resolution, 1),                                           \
        .steps = DT_INST_PROP_OR(n, steps, 0),                                                     \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, ec11c_init, NULL, &ec11c_data_##n, &ec11c_cfg_##n, POST_KERNEL,          \
                          CONFIG_SENSOR_INIT_PRIORITY, &ec11c_driver_api);

DT_INST_FOREACH_STATUS_OKAY(EC11_CLASSIC_INST)
