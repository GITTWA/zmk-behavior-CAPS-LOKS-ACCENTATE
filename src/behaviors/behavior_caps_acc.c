/*
 * Copyright (c) 2026 GITTWA
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_caps_acc

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <dt-bindings/zmk/keys.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <zmk/hid_indicators.h>
#include <zmk/hid_indicators_types.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define ZMK_LED_CAPSLOCK_BIT BIT(1)
#define ALT_CODE_DIGITS 4

struct behavior_caps_acc_config {
    struct zmk_behavior_binding normal_binding;
    uint16_t alt_code;
    bool has_alt_code_shifted;
    uint16_t alt_code_shifted;
    uint32_t tap_ms;
    uint32_t wait_ms;
};

struct behavior_caps_acc_data {
    const struct device *dev;
    struct k_work_delayable work;
    uint32_t digits[ALT_CODE_DIGITS];
    uint8_t step;
    bool busy;
    bool masked_shift;
    bool normal_pressed;
};

static uint32_t digit_to_kp(uint8_t digit) {
    switch (digit) {
    case 0:
        return KP_N0;
    case 1:
        return KP_N1;
    case 2:
        return KP_N2;
    case 3:
        return KP_N3;
    case 4:
        return KP_N4;
    case 5:
        return KP_N5;
    case 6:
        return KP_N6;
    case 7:
        return KP_N7;
    case 8:
        return KP_N8;
    case 9:
        return KP_N9;
    default:
        return KP_N0;
    }
}

static void fill_digits(uint16_t code, uint32_t out[ALT_CODE_DIGITS]) {
    out[0] = digit_to_kp((code / 1000) % 10);
    out[1] = digit_to_kp((code / 100) % 10);
    out[2] = digit_to_kp((code / 10) % 10);
    out[3] = digit_to_kp(code % 10);
}

static void caps_acc_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct behavior_caps_acc_data *data =
        CONTAINER_OF(dwork, struct behavior_caps_acc_data, work);
    const struct behavior_caps_acc_config *cfg = data->dev->config;
    int64_t now = k_uptime_get();

    if (data->step < ALT_CODE_DIGITS * 2) {
        uint8_t idx = data->step / 2;
        bool press = (data->step % 2) == 0;

        raise_zmk_keycode_state_changed_from_encoded(data->digits[idx], press, now);
        data->step++;

        uint32_t delay = press ? cfg->tap_ms : cfg->wait_ms;
        k_work_reschedule(&data->work, K_MSEC(delay));
    } else {
        raise_zmk_keycode_state_changed_from_encoded(LALT, false, now);
        if (data->masked_shift) {
            zmk_hid_masked_modifiers_clear();
            data->masked_shift = false;
        }
        data->busy = false;
    }
}

static int on_caps_acc_binding_pressed(struct zmk_behavior_binding *binding,
                                       struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (dev == NULL) {
        return -ENODEV;
    }

    const struct behavior_caps_acc_config *cfg = dev->config;
    struct behavior_caps_acc_data *data = dev->data;

    if (data->busy) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    zmk_hid_indicators_t indicators = zmk_hid_indicators_get_current_profile();
    bool caps_on = (indicators & ZMK_LED_CAPSLOCK_BIT) != 0;
    bool shift_on = (zmk_hid_get_explicit_mods() & (MOD_LSFT | MOD_RSFT)) != 0;

    if (caps_on && (!shift_on || cfg->has_alt_code_shifted)) {
        uint16_t code = shift_on ? cfg->alt_code_shifted : cfg->alt_code;
        fill_digits(code, data->digits);
        data->step = 0;
        data->busy = true;
        data->normal_pressed = false;

        if (shift_on) {
            zmk_hid_masked_modifiers_set(MOD_LSFT | MOD_RSFT);
            data->masked_shift = true;
        } else {
            data->masked_shift = false;
        }

        raise_zmk_keycode_state_changed_from_encoded(LALT, true, event.timestamp);
        k_work_reschedule(&data->work, K_MSEC(cfg->wait_ms));
        return ZMK_BEHAVIOR_OPAQUE;
    }

    data->normal_pressed = true;
    return zmk_behavior_invoke_binding(&cfg->normal_binding, event, true);
}

static int on_caps_acc_binding_released(struct zmk_behavior_binding *binding,
                                        struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (dev == NULL) {
        return -ENODEV;
    }

    const struct behavior_caps_acc_config *cfg = dev->config;
    struct behavior_caps_acc_data *data = dev->data;

    if (data->normal_pressed) {
        data->normal_pressed = false;
        return zmk_behavior_invoke_binding(&cfg->normal_binding, event, false);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int behavior_caps_acc_init(const struct device *dev) {
    struct behavior_caps_acc_data *data = dev->data;
    data->dev = dev;
    k_work_init_delayable(&data->work, caps_acc_work_handler);
    return 0;
}

static const struct behavior_driver_api behavior_caps_acc_driver_api = {
    .binding_pressed = on_caps_acc_binding_pressed,
    .binding_released = on_caps_acc_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define _TRANSFORM_BINDING(n)                                                               \
    {                                                                                       \
        .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, bindings, 0)),            \
        .param1 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(n, bindings, 0, param1), (0),    \
                              (DT_INST_PHA_BY_IDX(n, bindings, 0, param1))),                \
        .param2 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(n, bindings, 0, param2), (0),    \
                              (DT_INST_PHA_BY_IDX(n, bindings, 0, param2))),                \
    }

#define CAPS_ACC_INST(n)                                                                    \
    static struct behavior_caps_acc_data behavior_caps_acc_data_##n = {};                   \
    static const struct behavior_caps_acc_config behavior_caps_acc_config_##n = {           \
        .normal_binding = _TRANSFORM_BINDING(n),                                            \
        .alt_code = DT_INST_PROP(n, alt_code),                                              \
        .has_alt_code_shifted = DT_INST_NODE_HAS_PROP(n, alt_code_shifted),                 \
        .alt_code_shifted = DT_INST_PROP_OR(n, alt_code_shifted, 0),                        \
        .tap_ms = DT_INST_PROP(n, tap_ms),                                                  \
        .wait_ms = DT_INST_PROP(n, wait_ms),                                                \
    };                                                                                      \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_caps_acc_init, NULL,                                \
                            &behavior_caps_acc_data_##n,                                    \
                            &behavior_caps_acc_config_##n,                                  \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,               \
                            &behavior_caps_acc_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CAPS_ACC_INST)
