/*
 * Input processor that invokes behaviors for matched events without consuming
 * the original input event.
 */

#define DT_DRV_COMPAT zmk_input_processor_behavior_trigger

#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <drivers/input_processor.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zmk/virtual_key_position.h>

struct ip_behavior_trigger_config {
    uint8_t index;
    size_t code_count;
    size_t binding_count;
    uint16_t type;

    const uint16_t *codes;
    const struct zmk_behavior_binding *bindings;
    const struct zmk_behavior_binding *timeout_binding;
    uint32_t timeout_ms;
};

struct ip_behavior_trigger_data {
    struct k_work_delayable timeout_work;
    const struct device *dev;
    uint8_t input_device_index;
};

static void ip_behavior_trigger_timeout_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct ip_behavior_trigger_data *data =
        CONTAINER_OF(dwork, struct ip_behavior_trigger_data, timeout_work);
    const struct ip_behavior_trigger_config *cfg = data->dev->config;

    if (cfg->timeout_binding == NULL) {
        return;
    }

    struct zmk_behavior_binding_event behavior_event = {
        .position = ZMK_VIRTUAL_KEY_POSITION_BEHAVIOR_INPUT_PROCESSOR(data->input_device_index,
                                                                      cfg->index),
        .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };

    (void)zmk_behavior_invoke_binding(cfg->timeout_binding, behavior_event, true);
    (void)zmk_behavior_invoke_binding(cfg->timeout_binding, behavior_event, false);
}

static int ip_behavior_trigger_handle_event(const struct device *dev, struct input_event *event,
                                            uint32_t param1, uint32_t param2,
                                            struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);

    const struct ip_behavior_trigger_config *cfg = dev->config;
    struct ip_behavior_trigger_data *data = dev->data;

    if (event->type != cfg->type) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    for (size_t i = 0; i < cfg->code_count; i++) {
        if (cfg->codes[i] == event->code) {
            struct zmk_behavior_binding_event behavior_event = {
                .position = ZMK_VIRTUAL_KEY_POSITION_BEHAVIOR_INPUT_PROCESSOR(
                    state->input_device_index, cfg->index),
                .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
                .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
            };
            const size_t binding_index = cfg->binding_count == 1 ? 0 : i;
            const bool pressed = event->value != 0;

            if (cfg->timeout_binding != NULL && cfg->timeout_ms > 0) {
                data->input_device_index = state->input_device_index;
                (void)k_work_reschedule(&data->timeout_work, K_MSEC(cfg->timeout_ms));
            }

            const int ret = zmk_behavior_invoke_binding(&cfg->bindings[binding_index],
                                                        behavior_event, pressed);
            return ret < 0 ? ret : ZMK_INPUT_PROC_CONTINUE;
        }
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api ip_behavior_trigger_driver_api = {
    .handle_event = ip_behavior_trigger_handle_event,
};

static int ip_behavior_trigger_init(const struct device *dev) {
    struct ip_behavior_trigger_data *data = dev->data;

    data->dev = dev;
    k_work_init_delayable(&data->timeout_work, ip_behavior_trigger_timeout_work_handler);

    return 0;
}

#define IP_BEHAVIOR_TRIGGER_TIMEOUT_BINDINGS(n)                                                   \
    COND_CODE_1(DT_INST_NODE_HAS_PROP(n, timeout_bindings),                                       \
                (static const struct zmk_behavior_binding                                         \
                     ip_behavior_trigger_timeout_bindings_##n[] = {                               \
                         LISTIFY(DT_INST_PROP_LEN(n, timeout_bindings),                           \
                                 ZMK_KEYMAP_EXTRACT_BINDING, (, ), DT_DRV_INST(n))};),            \
                ())

#define IP_BEHAVIOR_TRIGGER_TIMEOUT_BINDING(n)                                                    \
    COND_CODE_1(DT_INST_NODE_HAS_PROP(n, timeout_bindings),                                       \
                (&ip_behavior_trigger_timeout_bindings_##n[0]), (NULL))

#define IP_BEHAVIOR_TRIGGER_INST(n)                                                               \
    static const uint16_t ip_behavior_trigger_codes_##n[] = DT_INST_PROP(n, codes);               \
    static const struct zmk_behavior_binding ip_behavior_trigger_bindings_##n[] = {               \
        LISTIFY(DT_INST_PROP_LEN(n, bindings), ZMK_KEYMAP_EXTRACT_BINDING, (, ), DT_DRV_INST(n))}; \
    IP_BEHAVIOR_TRIGGER_TIMEOUT_BINDINGS(n)                                                       \
    BUILD_ASSERT((ARRAY_SIZE(ip_behavior_trigger_bindings_##n) == 1) ||                           \
                     (ARRAY_SIZE(ip_behavior_trigger_codes_##n) ==                                \
                      ARRAY_SIZE(ip_behavior_trigger_bindings_##n)),                              \
                 "bindings must have either one entry or the same length as codes");              \
    BUILD_ASSERT(DT_INST_PROP_OR(n, timeout_ms, 0) == 0 ||                                        \
                     DT_INST_NODE_HAS_PROP(n, timeout_bindings),                                  \
                 "timeout-bindings is required when timeout-ms is non-zero");                    \
    static const struct ip_behavior_trigger_config ip_behavior_trigger_config_##n = {              \
        .index = n,                                                                               \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_KEY),                                           \
        .code_count = DT_INST_PROP_LEN(n, codes),                                                 \
        .binding_count = DT_INST_PROP_LEN(n, bindings),                                           \
        .codes = ip_behavior_trigger_codes_##n,                                                   \
        .bindings = ip_behavior_trigger_bindings_##n,                                             \
        .timeout_binding = IP_BEHAVIOR_TRIGGER_TIMEOUT_BINDING(n),                                \
        .timeout_ms = DT_INST_PROP_OR(n, timeout_ms, 0),                                          \
    };                                                                                            \
    static struct ip_behavior_trigger_data ip_behavior_trigger_data_##n;                           \
    DEVICE_DT_INST_DEFINE(n, &ip_behavior_trigger_init, NULL, &ip_behavior_trigger_data_##n,      \
                          &ip_behavior_trigger_config_##n, POST_KERNEL,                           \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &ip_behavior_trigger_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IP_BEHAVIOR_TRIGGER_INST)
