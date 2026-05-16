/*
 * Input processor that selects a layer on activity, then selects another
 * layer after a period without matching input events.
 */

#define DT_DRV_COMPAT zmk_input_processor_layer_timeout

#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <drivers/input_processor.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/keymap.h>

struct layer_timeout_config {
    uint8_t index;
    uint8_t layer;
    uint8_t timeout_layer;
    uint16_t type;
    uint32_t timeout_ms;
    size_t code_count;
    const uint16_t *codes;
};

struct layer_timeout_data {
    struct k_work_delayable timeout_work;
    const struct device *dev;
};

/* Motion and button processors share one inactivity window for the trackpad. */
static int64_t last_activity_ms;

static void layer_timeout_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct layer_timeout_data *data =
        CONTAINER_OF(dwork, struct layer_timeout_data, timeout_work);
    const struct layer_timeout_config *cfg = data->dev->config;
    int64_t elapsed = k_uptime_get() - last_activity_ms;

    if (elapsed < cfg->timeout_ms) {
        uint32_t remaining = cfg->timeout_ms - (uint32_t)elapsed;
        (void)k_work_reschedule(&data->timeout_work, K_MSEC(remaining));
        LOG_DBG("layer-timeout[%u]: timeout deferred, remaining=%u ms", cfg->index, remaining);
        return;
    }

    int rc = zmk_keymap_layer_to(cfg->timeout_layer);
    LOG_INF("layer-timeout[%u]: timeout fired -> layer %u rc=%d", cfg->index,
            cfg->timeout_layer, rc);
}

static int layer_timeout_handle_event(const struct device *dev, struct input_event *event,
                                      uint32_t param1, uint32_t param2,
                                      struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    const struct layer_timeout_config *cfg = dev->config;
    struct layer_timeout_data *data = dev->data;

    if (event->type != cfg->type) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    for (size_t i = 0; i < cfg->code_count; i++) {
        if (cfg->codes[i] != event->code) {
            continue;
        }

        LOG_INF("layer-timeout[%u]: event type=%u code=%u value=%d", cfg->index, event->type,
                event->code, event->value);

        if (event->type == INPUT_EV_REL && event->value == 0) {
            LOG_DBG("layer-timeout[%u]: ignored zero REL event", cfg->index);
            return ZMK_INPUT_PROC_CONTINUE;
        }

        int rc = zmk_keymap_layer_to(cfg->layer);
        LOG_INF("layer-timeout[%u]: activity -> layer %u rc=%d", cfg->index, cfg->layer, rc);

        last_activity_ms = k_uptime_get();
        (void)k_work_reschedule(&data->timeout_work, K_MSEC(cfg->timeout_ms));
        LOG_DBG("layer-timeout[%u]: timeout scheduled in %u ms", cfg->index, cfg->timeout_ms);

        return ZMK_INPUT_PROC_CONTINUE;
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api layer_timeout_driver_api = {
    .handle_event = layer_timeout_handle_event,
};

static int layer_timeout_init(const struct device *dev) {
    const struct layer_timeout_config *cfg = dev->config;
    struct layer_timeout_data *data = dev->data;

    data->dev = dev;
    k_work_init_delayable(&data->timeout_work, layer_timeout_work_handler);
    LOG_INF("layer-timeout[%u]: init layer=%u timeout-layer=%u timeout-ms=%u", cfg->index,
            cfg->layer, cfg->timeout_layer, cfg->timeout_ms);

    return 0;
}

#define LAYER_TIMEOUT_INST(n)                                                                      \
    static const uint16_t layer_timeout_codes_##n[] = DT_INST_PROP(n, codes);                      \
    static const struct layer_timeout_config layer_timeout_config_##n = {                           \
        .index = n,                                                                                \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_KEY),                                            \
        .code_count = DT_INST_PROP_LEN(n, codes),                                                  \
        .codes = layer_timeout_codes_##n,                                                          \
        .layer = DT_INST_PROP(n, layer),                                                           \
        .timeout_layer = DT_INST_PROP(n, timeout_layer),                                           \
        .timeout_ms = DT_INST_PROP(n, timeout_ms),                                                 \
    };                                                                                             \
    static struct layer_timeout_data layer_timeout_data_##n;                                       \
    DEVICE_DT_INST_DEFINE(n, &layer_timeout_init, NULL, &layer_timeout_data_##n,                   \
                          &layer_timeout_config_##n, POST_KERNEL,                                  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &layer_timeout_driver_api);

DT_INST_FOREACH_STATUS_OKAY(LAYER_TIMEOUT_INST)
