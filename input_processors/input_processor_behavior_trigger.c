/*
 * Input processor that invokes behaviors for matched events without consuming
 * the original input event.
 */

#define DT_DRV_COMPAT zmk_input_processor_behavior_trigger

#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
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
};

static int ip_behavior_trigger_handle_event(const struct device *dev, struct input_event *event,
                                            uint32_t param1, uint32_t param2,
                                            struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);

    const struct ip_behavior_trigger_config *cfg = dev->config;

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

            return zmk_behavior_invoke_binding(&cfg->bindings[binding_index], behavior_event,
                                               pressed);
        }
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api ip_behavior_trigger_driver_api = {
    .handle_event = ip_behavior_trigger_handle_event,
};

static int ip_behavior_trigger_init(const struct device *dev) { return 0; }

#define IP_BEHAVIOR_TRIGGER_INST(n)                                                               \
    static const uint16_t ip_behavior_trigger_codes_##n[] = DT_INST_PROP(n, codes);               \
    static const struct zmk_behavior_binding ip_behavior_trigger_bindings_##n[] = {               \
        LISTIFY(DT_INST_PROP_LEN(n, bindings), ZMK_KEYMAP_EXTRACT_BINDING, (, ), DT_DRV_INST(n))}; \
    BUILD_ASSERT((ARRAY_SIZE(ip_behavior_trigger_bindings_##n) == 1) ||                           \
                     (ARRAY_SIZE(ip_behavior_trigger_codes_##n) ==                                \
                      ARRAY_SIZE(ip_behavior_trigger_bindings_##n)),                              \
                 "bindings must have either one entry or the same length as codes");              \
    static const struct ip_behavior_trigger_config ip_behavior_trigger_config_##n = {              \
        .index = n,                                                                               \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_KEY),                                           \
        .code_count = DT_INST_PROP_LEN(n, codes),                                                 \
        .binding_count = DT_INST_PROP_LEN(n, bindings),                                           \
        .codes = ip_behavior_trigger_codes_##n,                                                   \
        .bindings = ip_behavior_trigger_bindings_##n,                                             \
    };                                                                                            \
    DEVICE_DT_INST_DEFINE(n, &ip_behavior_trigger_init, NULL, NULL,                               \
                          &ip_behavior_trigger_config_##n, POST_KERNEL,                           \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &ip_behavior_trigger_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IP_BEHAVIOR_TRIGGER_INST)
