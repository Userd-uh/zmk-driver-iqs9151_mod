#define DT_DRV_COMPAT zmk_behavior_trackpad_to_pos

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/events/position_state_changed.h>

LOG_MODULE_REGISTER(trackpad_to_pos, LOG_LEVEL_INF);

static int behavior_trackpad_to_pos_raise(bool state,
                                          struct zmk_behavior_binding *binding,
                                          struct zmk_behavior_binding_event event) {
    LOG_INF("tp_to_pos pos=%d state=%d", binding->param1, state);

    struct zmk_position_state_changed ev = {
        .position = binding->param1,
        .state = state,
        .timestamp = k_uptime_get(),
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
    };

    ARG_UNUSED(event);
    return raise_zmk_position_state_changed(ev);
}

static int behavior_trackpad_to_pos_binding_pressed(
    struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    return behavior_trackpad_to_pos_raise(true, binding, event);
}

static int behavior_trackpad_to_pos_binding_released(
    struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    return behavior_trackpad_to_pos_raise(false, binding, event);
}

static const struct behavior_driver_api behavior_trackpad_to_pos_driver_api = {
    .binding_pressed = behavior_trackpad_to_pos_binding_pressed,
    .binding_released = behavior_trackpad_to_pos_binding_released,
};

// Allow multiple instances if needed.
#define TP_TO_POS_INST(n)                                                                          \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                  \
                            &behavior_trackpad_to_pos_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TP_TO_POS_INST)
