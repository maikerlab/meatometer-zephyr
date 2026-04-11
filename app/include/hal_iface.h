#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    /* Measurement is in progress */
    LED_MEASURING = 0,
    /* Status LED - On: WiFi connected, Blinking: Target temperature reached */
    LED_STATUS,
    LED_COUNT,
} led_id_t;

/* ── Buttons ─────────────────────────────────────────────────────────── */

typedef enum
{
    /* Start/Stop measurement */
    BTN_MEASURE = 0,
    /* Reconnect Wi-Fi (start BLE advertising) */
    BTN_RECONNECT_WIFI,
    BTN_COUNT,
} btn_id_t;

/** Is called by HAL if a button was pressed. */
typedef void (*btn_callback_t)(btn_id_t btn);

/**
 * Abstract hardware interface.
 * In production code filled with real drivers, in tests with hal_mock.c
 */
typedef struct
{
    /** Initializes the hardware interface.
     * @return 0 on success, negative error code on failure
     */
    int (*init)(void);

    /** Reads temperaturein °C.
     * @param out_celsius Pointer to store the read temperature value
     * @return 0 on success, negative error code on failure
     */
    int (*read_temp)(float *out_celsius);

    /** Sets the state of a LED.
     * @param id LED identifier
     * @param on true to turn on, false to turn off
     */
    void (*led_set)(led_id_t id, bool on);
    /** Toggles the state of a LED.
     * @param id LED identifier
     */
    void (*led_toggle)(led_id_t id);
    /** Blinks a LED.
     * @param id LED identifier
     * @param period_ms Blink period in milliseconds
     */
    void (*led_blink)(led_id_t id, uint32_t period_ms);
    /** Turns off all LEDs. */
    void (*led_all_off)(void);

    /* Registers callback for all button events */
    void (*btn_register_callback)(btn_callback_t cb);
} hal_iface_t;
