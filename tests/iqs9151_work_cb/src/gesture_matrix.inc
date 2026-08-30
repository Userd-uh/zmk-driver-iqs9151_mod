/* Runs the actual driver frame path for every finger count, axis and sign. */
ZTEST_F(iqs9151_work_cb, test_mode_axis_direction_and_staged_release) {
    for (int fingers = 2; fingers <= 3; fingers++) {
        for (int axis = 0; axis < 2; axis++) {
            for (int sign = -1; sign <= 1; sign += 2) {
                iqs9151_test_context_init(fixture->ctx, NULL);
                fixture->log.count = 0;
                uint16_t flags = IQS9151_TP_FINGER1_CONFIDENCE |
                                 IQS9151_TP_FINGER2_CONFIDENCE | fingers;
                struct iqs9151_test_frame frame = make_frame(fingers, flags, 0, 0, 0,
                                                             1000, 1000, 1200, 1000);
                iqs9151_test_process_frame(fixture->ctx, &frame, k_uptime_get());
                for (int step = 0; step < 2; step++) {
                    if (axis == 0) { frame.finger1_x += sign * 400; frame.finger2_x += sign * 400; }
                    else { frame.finger1_y += sign * 400; frame.finger2_y += sign * 400; }
                    iqs9151_test_process_frame(fixture->ctx, &frame, k_uptime_get());
                }
                for (int tail = fingers - 1; tail >= 0; tail--) {
                    frame.finger_count = tail;
                    frame.trackpad_flags = tail ? (IQS9151_TP_FINGER1_CONFIDENCE | tail) : 0;
                    frame.rel_x = 20;
                    iqs9151_test_process_frame(fixture->ctx, &frame, k_uptime_get());
                }
                if (IQS_GESTURE_MATRIX == 0) {
                    zassert_equal(fixture->log.count, 0, "Disabled gesture emitted input");
                } else if (IQS_GESTURE_MATRIX == 1) {
                    zassert_equal(fixture->log.count, 2, "Scroll must emit both movement samples only");
                    for (size_t i = 0; i < fixture->log.count; i++) {
                        zassert_equal(fixture->log.events[i].type, IQS9151_TEST_EVENT_REL);
                        zassert_equal(fixture->log.events[i].code, axis ? INPUT_REL_WHEEL : INPUT_REL_HWHEEL);
                        zassert_equal(fixture->log.events[i].value, (axis ? sign : -sign) * 400);
                    }
                } else {
                    uint16_t expected = fingers == 2
                        ? (axis ? (sign < 0 ? IQS9151_BTN_2F_UP : IQS9151_BTN_2F_DOWN)
                                : (sign < 0 ? IQS9151_BTN_2F_LEFT : IQS9151_BTN_2F_RIGHT))
                        : (axis ? (sign < 0 ? IQS9151_BTN_3F_UP : IQS9151_BTN_3F_DOWN)
                                : (sign < 0 ? IQS9151_BTN_3F_LEFT : IQS9151_BTN_3F_RIGHT));
                    zassert_equal(fixture->log.count, 2, "Action must emit exactly one press/release");
                    for (size_t i = 0; i < 2; i++) {
                        zassert_equal(fixture->log.events[i].type, IQS9151_TEST_EVENT_KEY);
                        zassert_equal(fixture->log.events[i].code, expected);
                        zassert_equal(fixture->log.events[i].value, i == 0);
                    }
                }
            }
        }
    }
}
