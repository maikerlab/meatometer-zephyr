#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Abstract sensor interface.
 * In production code filled with real drivers, in tests with sensor_mock.c
 */
typedef struct {
  /** Initialize the sensor(s).
   * @return 0 on success, negative error code on failure.
   */
  int (*init)(void);
  /** Reads the current temperature in °C.
   * @param out_celsius Pointer to store the read temperature value
   * @return 0 on success, negative error code on failure
   */
  int (*read_temp)(float *out_celsius);
} sensor_iface_t;
