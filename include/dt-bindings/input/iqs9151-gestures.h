#ifndef IQS9151_GESTURES_H
#define IQS9151_GESTURES_H

/* Preserve legacy 2F codes; give 3F its own input button codes.
 * 0x2c0..0x2c3 are the Linux/Zephyr trigger-happy button range. */
#define IQS9151_BTN_2F_RIGHT 0x103
#define IQS9151_BTN_2F_LEFT  0x104
#define IQS9151_BTN_2F_UP    0x105
#define IQS9151_BTN_2F_DOWN  0x106
#define IQS9151_BTN_3F_RIGHT 0x2c0
#define IQS9151_BTN_3F_LEFT  0x2c1
#define IQS9151_BTN_3F_UP    0x2c2
#define IQS9151_BTN_3F_DOWN  0x2c3

#endif
