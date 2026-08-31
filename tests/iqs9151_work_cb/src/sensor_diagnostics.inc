ZTEST_F(iqs9151_work_cb, test_scroll_report_failure_is_observed_without_retry) {
    iqs9151_test_set_event_hook(NULL, NULL);
    report_return_code = -EAGAIN;
    struct iqs9151_test_frame frame = scroll_frame(3);
    iqs9151_test_process_frame(fixture->ctx, &frame, 0);
    scroll_move(&frame, 0, 60);
    iqs9151_test_process_frame(fixture->ctx, &frame, 10);
    zassert_equal(report_calls, 1, "Observation must not retry/drop/duplicate reports");
}

/* Exercise the production raw decoder, not just hand-built frame structs. */
ZTEST_F(iqs9151_work_cb, test_raw_sensor_diagnostics_and_settings_snapshot) {
    uint8_t raw[72] = {0};
    const uint8_t config[20] = {
        0x28, 12, 13, 3, 0x99, 0x09, 0, 12, 30, 0, 0xff, 1, 20, 20, 5, 3, 20, 20, 2, 20
    };
    /* Independent literal register offsets: little-endian data, all flag bits. */
    sys_put_le16(0x0200, raw + 12);
    sys_put_le16(0x45e3, raw + 14);
    for (int slot = 0; slot < 7; slot++) {
        uint8_t *finger = raw + 16 + 8 * slot;
        sys_put_le16(257 + slot, finger);
        sys_put_le16(513 + slot, finger + 2);
        sys_put_le16(0x1230 + slot, finger + 4);
        sys_put_le16(20 + slot, finger + 6);
    }
    sys_put_le16(UINT16_MAX, raw + 24);
    sys_put_le16(UINT16_MAX, raw + 26);
    iqs9151_test_set_diagnostic_config(fixture->ctx, config, 0);
    zassert_equal(iqs9151_test_process_raw(fixture->ctx, raw, 28, 12344), -EINVAL);
    zassert_equal(iqs9151_test_process_raw(fixture->ctx, raw, sizeof(raw), 12345), 0);
    zassert_equal(iqs9151_test_prev_finger_count(fixture->ctx), 3);
    zassert_equal(fixture->log.count, 0, "Observation must not introduce input events");
}

ZTEST_F(iqs9151_work_cb, test_failed_settings_readback_is_not_reported_as_defaults) {
    uint8_t raw[72] = {0};
    sys_put_le16(0x0101, raw + 14);
    iqs9151_test_set_diagnostic_config(fixture->ctx, NULL, -EIO);
    zassert_equal(iqs9151_test_process_raw(fixture->ctx, raw, sizeof(raw), 12346), 0);
    zassert_equal(iqs9151_test_prev_finger_count(fixture->ctx), 1);
}
