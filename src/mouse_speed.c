/*
 * SPDX-License-Identifier: MIT
 *
 * Mouse key speed levels.
 *
 * &mouse_speed steps the level 1 -> 2 -> ... -> CONFIG_ZMK_MOUSE_SPEED_LEVELS -> 1, like
 * DPI_MOD on a QMK Charybdis. A zmk,input-processor-mouse-speed processor turns the sensor's
 * counts (taken at its sensor-cpi) into the CPI of the current level, from its cpi-levels list.
 * The processor's parameter: 0 uses the current level's CPI, any other value is a fixed CPI
 * (used for sniping). The level is saved to flash, so it survives a restart.
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <drivers/input_processor.h>
#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define LEVELS CONFIG_ZMK_MOUSE_SPEED_LEVELS

BUILD_ASSERT(CONFIG_ZMK_MOUSE_SPEED_DEFAULT_LEVEL >= 1 &&
                 CONFIG_ZMK_MOUSE_SPEED_DEFAULT_LEVEL <= CONFIG_ZMK_MOUSE_SPEED_LEVELS,
             "The default mouse speed level must be between 1 and the number of levels");

static uint8_t level = CONFIG_ZMK_MOUSE_SPEED_DEFAULT_LEVEL;

#if IS_ENABLED(CONFIG_SETTINGS)

static void save_level(struct k_work *work) {
    int err = settings_save_one("mouse_speed/level", &level, sizeof(level));
    if (err < 0) {
        LOG_ERR("Failed to save the mouse speed level (%d)", err);
    }
}

static K_WORK_DELAYABLE_DEFINE(save_level_work, save_level);

static int mouse_speed_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                    void *cb_arg) {
    const char *next;
    uint8_t saved;

    if (!settings_name_steq(name, "level", &next) || next) {
        return -ENOENT;
    }
    if (len != sizeof(saved) || read_cb(cb_arg, &saved, sizeof(saved)) != sizeof(saved)) {
        return -EINVAL;
    }
    if (saved >= 1 && saved <= LEVELS) {
        level = saved;
    }
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(mouse_speed, "mouse_speed", NULL, mouse_speed_settings_set, NULL,
                               NULL);

#endif /* IS_ENABLED(CONFIG_SETTINGS) */

/* The behavior that steps the level */

#define DT_DRV_COMPAT zmk_behavior_mouse_speed

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int mouse_speed_binding_pressed(struct zmk_behavior_binding *binding,
                                       struct zmk_behavior_binding_event event) {
    level = level % LEVELS + 1;
    LOG_DBG("Mouse speed level %d of %d", level, LEVELS);

#if IS_ENABLED(CONFIG_SETTINGS)
    k_work_reschedule(&save_level_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
#endif
    return ZMK_BEHAVIOR_OPAQUE;
}

static int mouse_speed_binding_released(struct zmk_behavior_binding *binding,
                                        struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api mouse_speed_driver_api = {
    .binding_pressed = mouse_speed_binding_pressed,
    .binding_released = mouse_speed_binding_released,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &mouse_speed_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */

/* The input processor that applies the level, modeled on ZMK's input_processor_scaler.c */

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT zmk_input_processor_mouse_speed

struct mouse_speed_scaler_config {
    uint8_t type;
    uint16_t sensor_cpi;
    const uint16_t *cpi_levels;
    size_t codes_len;
    uint16_t codes[];
};

static int mouse_speed_handle_event(const struct device *dev, struct input_event *event,
                                    uint32_t param1, uint32_t param2,
                                    struct zmk_input_processor_state *state) {
    const struct mouse_speed_scaler_config *cfg = dev->config;

    if (event->type != cfg->type) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    for (size_t i = 0; i < cfg->codes_len; i++) {
        if (cfg->codes[i] != event->code) {
            continue;
        }

        int32_t cpi = param1 ? param1 : cfg->cpi_levels[level - 1];

        // Keep the remainder, so slow movements are not lost at the low levels
        int32_t value = event->value * cpi;
        if (state && state->remainder) {
            value += *state->remainder;
        }
        int32_t scaled = value / cfg->sensor_cpi;
        if (state && state->remainder) {
            *state->remainder = value - scaled * cfg->sensor_cpi;
        }
        event->value = scaled;
        break;
    }
    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api mouse_speed_scaler_driver_api = {
    .handle_event = mouse_speed_handle_event,
};

#define MOUSE_SPEED_SCALER_INST(n)                                                                 \
    BUILD_ASSERT(DT_INST_PROP_LEN(n, cpi_levels) == LEVELS,                                        \
                 "cpi-levels needs one CPI per mouse speed level");                                 \
    static const uint16_t mouse_speed_cpi_levels_##n[] = DT_INST_PROP(n, cpi_levels);              \
    static const struct mouse_speed_scaler_config mouse_speed_scaler_config_##n = {                \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_REL),                                            \
        .sensor_cpi = DT_INST_PROP(n, sensor_cpi),                                                 \
        .cpi_levels = mouse_speed_cpi_levels_##n,                                                  \
        .codes_len = DT_INST_PROP_LEN(n, codes),                                                   \
        .codes = DT_INST_PROP(n, codes),                                                           \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &mouse_speed_scaler_config_##n, POST_KERNEL,        \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &mouse_speed_scaler_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MOUSE_SPEED_SCALER_INST)
