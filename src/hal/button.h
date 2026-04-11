// src/hal/button.h
#pragma once

#include "hal_iface.h"

int button_init(void);
void button_register_callback(btn_callback_t cb);