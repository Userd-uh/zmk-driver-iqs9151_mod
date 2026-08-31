static struct iqs9151_test_frame scroll_frame(uint8_t fingers) {
    return make_frame(fingers, IQS9151_TP_FINGER1_CONFIDENCE |
                      IQS9151_TP_FINGER2_CONFIDENCE | fingers,
                      0, 0, 0, 1000, 1000, 1200, 1000);
}

static void scroll_move(struct iqs9151_test_frame *frame, int axis, int delta) {
    if (axis == 0) {
        frame->finger1_x += delta;
        frame->finger2_x += delta;
    } else {
        frame->finger1_y += delta;
        frame->finger2_y += delta;
    }
}

ZTEST_F(iqs9151_work_cb, test_small_steps_match_two_finger_output) {
    for (int axis = 0; axis < 2; axis++) {
        for (int sign = -1; sign <= 1; sign += 2) {
            for (int fingers = 2; fingers <= 3; fingers++) {
                iqs9151_work_cb_before(fixture);
                struct iqs9151_test_frame frame = scroll_frame(fingers);
                iqs9151_test_process_frame(fixture->ctx, &frame, 0);
                scroll_move(&frame, axis, sign * 60);
                iqs9151_test_process_frame(fixture->ctx, &frame, 10);
                fixture->log.count = 0;
                for (int i = 0; i < 20; i++) {
                    scroll_move(&frame, axis, sign);
                    iqs9151_test_process_frame(fixture->ctx, &frame, 11 + i);
                }
                zassert_equal(fixture->log.count, 20, "Small steps must not be dropped or batched");
                for (size_t i = 0; i < fixture->log.count; i++) {
                    zassert_equal(fixture->log.events[i].type, IQS9151_TEST_EVENT_REL);
                    zassert_equal(fixture->log.events[i].code, axis ? INPUT_REL_WHEEL : INPUT_REL_HWHEEL);
                    zassert_equal(fixture->log.events[i].value, axis ? sign : -sign);
                    zassert_true(fixture->log.events[i].sync);
                }
            }
        }
    }
}

ZTEST_F(iqs9151_work_cb, test_three_finger_release_uses_scroll_inertia_and_new_touch_cancels) {
    struct iqs9151_test_frame frame = scroll_frame(3);
    iqs9151_test_process_frame(fixture->ctx, &frame, 0);
    scroll_move(&frame, 0, 60);
    iqs9151_test_process_frame(fixture->ctx, &frame, 10);
    frame = scroll_frame(0);
    iqs9151_test_process_frame(fixture->ctx, &frame, 20);
    zassert_true(iqs9151_test_scroll_inertia_active(fixture->ctx), "3F must seed shared inertia");
    fixture->log.count = 0;
    k_sleep(K_MSEC(20));
    zassert_true(fixture->log.count > 0, "Shared worker must continue wheel output");
    for (size_t i = 0; i < fixture->log.count; i++) {
        zassert_equal(fixture->log.events[i].code, INPUT_REL_HWHEEL);
        zassert_true(fixture->log.events[i].value < 0);
    }
    frame = scroll_frame(3);
    iqs9151_test_process_frame(fixture->ctx, &frame, 40);
    zassert_false(iqs9151_test_scroll_inertia_active(fixture->ctx));
    zassert_false(iqs9151_test_cursor_inertia_active(fixture->ctx));
}

ZTEST_F(iqs9151_work_cb, test_three_finger_staged_release_has_one_inertia_start_no_pointer) {
    struct iqs9151_test_frame frame = scroll_frame(3);
    iqs9151_test_process_frame(fixture->ctx, &frame, 0);
    scroll_move(&frame, 1, 60);
    iqs9151_test_process_frame(fixture->ctx, &frame, 10);
    fixture->log.count = 0;
    for (int count = 2; count >= 0; count--) {
        frame.finger_count = count;
        frame.trackpad_flags = count | IQS9151_TP_MOVEMENT_DETECTED | IQS9151_TP_FINGER1_CONFIDENCE;
        frame.rel_x = 20;
        iqs9151_test_process_frame(fixture->ctx, &frame, 20 + (2-count)*5);
        zassert_equal(iqs9151_test_scroll_inertia_active(fixture->ctx), count == 0);
        zassert_false(iqs9151_test_cursor_inertia_active(fixture->ctx));
        zassert_equal(fixture->log.count, 0, "Release must not emit duplicate wheel/key/pointer input");
    }
    iqs9151_test_cancel_pending_work(fixture->ctx);
}

ZTEST_F(iqs9151_work_cb, test_three_finger_stale_release_does_not_fling) {
    struct iqs9151_test_frame frame = scroll_frame(3);
    iqs9151_test_process_frame(fixture->ctx, &frame, 0);
    scroll_move(&frame, 0, 60);
    iqs9151_test_process_frame(fixture->ctx, &frame, 10);
    frame = scroll_frame(0);
    iqs9151_test_process_frame(fixture->ctx, &frame, 100);
    zassert_false(iqs9151_test_scroll_inertia_active(fixture->ctx));
}

/* Observed tail timing, synthetic movement: snapshot at 50, start only at 110. */
ZTEST_F(iqs9151_work_cb, test_observed_staged_release_preserves_inertia_history) {
    struct iqs9151_test_frame frame = scroll_frame(3);
    iqs9151_test_process_frame(fixture->ctx, &frame, 0);
    scroll_move(&frame, 0, -60);
    iqs9151_test_process_frame(fixture->ctx, &frame, 10);
    scroll_move(&frame, 0, -143);
    iqs9151_test_process_frame(fixture->ctx, &frame, 23);
    scroll_move(&frame, 0, -165);
    iqs9151_test_process_frame(fixture->ctx, &frame, 37);
    fixture->log.count = 0;
    const uint8_t counts[] = {2, 1, 0};
    const int64_t times[] = {50, 75, 110};
    for (size_t i = 0; i < ARRAY_SIZE(counts); i++) {
        frame.finger_count = counts[i];
        frame.trackpad_flags = counts[i] | IQS9151_TP_FINGER1_CONFIDENCE;
        iqs9151_test_process_frame(fixture->ctx, &frame, times[i]);
        zassert_equal(iqs9151_test_scroll_inertia_active(fixture->ctx), counts[i] == 0);
        zassert_false(iqs9151_test_cursor_inertia_active(fixture->ctx));
        zassert_equal(fixture->log.count, 0, "Staged tail must not emit new input");
    }
    iqs9151_test_process_frame(fixture->ctx, &frame, 111);
    zassert_equal(fixture->log.count, 0, "Repeated idle must not emit release input");
    k_sleep(K_MSEC(20));
    zassert_true(fixture->log.count > 0, "Saved release must produce inertia after all up");
    for (size_t i = 0; i < fixture->log.count; i++) {
        zassert_equal(fixture->log.events[i].code, INPUT_REL_HWHEEL);
        zassert_true(fixture->log.events[i].value > 0);
    }
}

ZTEST_F(iqs9151_work_cb, test_three_finger_long_remaining_contact_keeps_release_snapshot) {
    struct iqs9151_test_frame frame = scroll_frame(3);
    iqs9151_test_process_frame(fixture->ctx, &frame, 0);
    scroll_move(&frame, 0, 60);
    iqs9151_test_process_frame(fixture->ctx, &frame, 10);
    frame.finger_count = 2;
    iqs9151_test_process_frame(fixture->ctx, &frame, 23);
    fixture->log.count = 0;
    frame.finger_count = 1;
    iqs9151_test_process_frame(fixture->ctx, &frame, 1000);
    k_sleep(K_MSEC(20));
    zassert_false(iqs9151_test_scroll_inertia_active(fixture->ctx));
    zassert_equal(fixture->log.count, 0);
    frame.finger_count = 0;
    iqs9151_test_process_frame(fixture->ctx, &frame, 5010);
    zassert_true(iqs9151_test_scroll_inertia_active(fixture->ctx));
    zassert_equal(fixture->log.count, 0);
    iqs9151_test_cancel_pending_work(fixture->ctx);
}

ZTEST_F(iqs9151_work_cb, test_three_finger_resume_discards_old_fling_and_allows_fresh_motion) {
    for (int fresh = 0; fresh <= 1; fresh++) {
        iqs9151_work_cb_before(fixture);
        struct iqs9151_test_frame frame = scroll_frame(3);
        iqs9151_test_process_frame(fixture->ctx, &frame, 0);
        scroll_move(&frame, 0, 60);
        iqs9151_test_process_frame(fixture->ctx, &frame, 10);
        frame.finger_count = 2;
        iqs9151_test_process_frame(fixture->ctx, &frame, 20);
        frame.finger_count = 3;
        iqs9151_test_process_frame(fixture->ctx, &frame, 25);
        fixture->log.count = 0;
        if (fresh) {
            scroll_move(&frame, 0, -60);
            iqs9151_test_process_frame(fixture->ctx, &frame, 30);
            fixture->log.count = 0;
        }
        frame.finger_count = 0;
        iqs9151_test_process_frame(fixture->ctx, &frame, 40);
        zassert_equal(iqs9151_test_scroll_inertia_active(fixture->ctx), fresh != 0);
        k_sleep(K_MSEC(20));
        if (fresh) {
            zassert_true(fixture->log.count > 0);
            for (size_t i = 0; i < fixture->log.count; i++) {
                zassert_equal(fixture->log.events[i].code, INPUT_REL_HWHEEL);
                zassert_true(fixture->log.events[i].value > 0, "Must use fresh opposite direction");
            }
        } else {
            zassert_equal(fixture->log.count, 0, "No stale candidate after recontact");
        }
    }
}

ZTEST_F(iqs9151_work_cb, test_remaining_finger_count_increase_invalidates_release) {
    struct iqs9151_test_frame frame = scroll_frame(3);
    iqs9151_test_process_frame(fixture->ctx, &frame, 0);
    scroll_move(&frame, 0, 60);
    iqs9151_test_process_frame(fixture->ctx, &frame, 10);
    fixture->log.count = 0;
    const uint8_t counts[] = {2, 1, 2, 0};
    for (size_t i = 0; i < ARRAY_SIZE(counts); i++) {
        frame.finger_count = counts[i];
        iqs9151_test_process_frame(fixture->ctx, &frame, 15 + i * 5);
        zassert_false(iqs9151_test_scroll_inertia_active(fixture->ctx));
        zassert_false(iqs9151_test_cursor_inertia_active(fixture->ctx));
        zassert_equal(fixture->log.count, 0);
    }
}

ZTEST_F(iqs9151_work_cb, test_three_finger_stale_before_first_drop_stays_rejected) {
    struct iqs9151_test_frame frame = scroll_frame(3);
    iqs9151_test_process_frame(fixture->ctx, &frame, 0);
    scroll_move(&frame, 0, 60);
    iqs9151_test_process_frame(fixture->ctx, &frame, 10);
    frame.finger_count = 2;
    iqs9151_test_process_frame(fixture->ctx, &frame, 46);
    frame.finger_count = 0;
    iqs9151_test_process_frame(fixture->ctx, &frame, 1000);
    zassert_false(iqs9151_test_scroll_inertia_active(fixture->ctx));
}

ZTEST_F(iqs9151_work_cb, test_two_finger_release_gate_is_unchanged) {
    for (int gap = 35; gap <= 36; gap++) {
        iqs9151_work_cb_before(fixture);
        struct iqs9151_test_frame frame = scroll_frame(2);
        iqs9151_test_process_frame(fixture->ctx, &frame, 0);
        scroll_move(&frame, 0, 60);
        iqs9151_test_process_frame(fixture->ctx, &frame, 10);
        frame = scroll_frame(0);
        iqs9151_test_process_frame(fixture->ctx, &frame, 10 + gap);
        zassert_equal(iqs9151_test_scroll_inertia_active(fixture->ctx), gap == 35);
        iqs9151_test_cancel_pending_work(fixture->ctx);
    }
}

ZTEST_F(iqs9151_work_cb, test_scroll_inertia_stale_gap_boundary) {
    for (int gap = 35; gap <= 36; gap++) {
        iqs9151_work_cb_before(fixture);
        struct iqs9151_test_frame frame = scroll_frame(3);
        iqs9151_test_process_frame(fixture->ctx, &frame, 0);
        scroll_move(&frame, 0, 60);
        iqs9151_test_process_frame(fixture->ctx, &frame, 10);
        frame = scroll_frame(0);
        iqs9151_test_process_frame(fixture->ctx, &frame, 10 + gap);
        zassert_equal(iqs9151_test_scroll_inertia_active(fixture->ctx), gap == 35);
        iqs9151_test_cancel_pending_work(fixture->ctx);
    }
}

ZTEST_F(iqs9151_work_cb, test_three_finger_resume_has_no_coordinate_jump) {
    struct iqs9151_test_frame frame = scroll_frame(3);
    iqs9151_test_process_frame(fixture->ctx, &frame, 0);
    scroll_move(&frame, 0, 60);
    iqs9151_test_process_frame(fixture->ctx, &frame, 10);
    frame.finger_count = 2;
    iqs9151_test_process_frame(fixture->ctx, &frame, 15);
    frame.finger_count = 3;
    scroll_move(&frame, 0, 500);
    fixture->log.count = 0;
    iqs9151_test_process_frame(fixture->ctx, &frame, 20);
    zassert_equal(fixture->log.count, 0);
    scroll_move(&frame, 0, 1);
    iqs9151_test_process_frame(fixture->ctx, &frame, 25);
    zassert_equal(fixture->log.count, 1);
    zassert_equal(fixture->log.events[0].value, -1);
}
