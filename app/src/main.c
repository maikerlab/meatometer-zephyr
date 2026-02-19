/*
 * Copyright (c) 2026 Maik Lorenz <maik.lorenz@pm.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/app_version.h>
#include "temperature/temp.h"

LOG_MODULE_REGISTER(main);

/* size of stack area used by each thread */
#define TH_STACKSIZE 1024
/* scheduling priority used by AppState thread */
#define TH_APPSTATE_PRIORITY 1
/* scheduling priority used by MeasureTemp thread */
#define TH_TEMP_PRIORITY 7

#define LED_PWR_NODE DT_ALIAS(led_power)
#define LED_STATUS_NODE DT_ALIAS(led_status)
#define BTN_PWR_NODE DT_ALIAS(btn_power)
#define BTN_START_NODE DT_ALIAS(btn_start)

/* Meat temperature sensor 1 */
static const struct device *const dev_temp0 = DEVICE_DT_GET(DT_ALIAS(temp0));

static const struct gpio_dt_spec led_power = GPIO_DT_SPEC_GET(LED_PWR_NODE, gpios);
static const struct gpio_dt_spec led_status = GPIO_DT_SPEC_GET(LED_STATUS_NODE, gpios);
static const struct gpio_dt_spec btn_power = GPIO_DT_SPEC_GET(BTN_PWR_NODE, gpios);
static const struct gpio_dt_spec btn_start = GPIO_DT_SPEC_GET(BTN_START_NODE, gpios);
static struct gpio_callback button_cb_data;

static bool is_measuring = false;

enum event_type
{
	BTN_PRESSED,
	TEMP_READY,
};
enum button_id
{
	BTN_POWER,
	BTN_START
};
struct app_event
{
	enum event_type event;
	union
	{
		struct
		{
			double temperature;
		} temp;
		struct
		{
			enum button_id id;
		} button;
	} data;
};

enum ctrl_type
{
	CMD_TEMP_READ,
};
struct ctrl_event
{
	enum ctrl_type cmd;
};

/* Button events */
K_MSGQ_DEFINE(app_msgq, sizeof(struct app_event), 10, 1);
/* Control messages */
K_MSGQ_DEFINE(ctrl_msgq, sizeof(struct ctrl_event), 10, 1);

void button_pressed(const struct device *dev, struct gpio_callback *cb,
					uint32_t pins)
{
	LOG_INF("Button %s, pin %d pressed at %" PRIu32 "", dev->name, pins, k_cycle_get_32());

	int ret;
	struct app_event data;
	if (pins & BIT(btn_power.pin))
	{
		LOG_INF("POWER Button pressed");
		data.event = BTN_PRESSED;
		data.data.button.id = BTN_POWER;
	}
	else if (pins & BIT(btn_start.pin))
	{
		LOG_INF("START Button pressed");
		data.event = BTN_PRESSED;
		data.data.button.id = BTN_START;
	}
	ret = k_msgq_put(&app_msgq, &data, K_FOREVER);
	if (ret < 0)
	{
		LOG_ERR("Error sending button event to app_msgq");
	}
}

static int init_hardware(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led_power))
	{
		LOG_ERR("Power LED not ready for use");
		return -1;
	}
	if (!gpio_is_ready_dt(&led_status))
	{
		LOG_ERR("Status LED not ready for use");
		return -1;
	}
	if (!gpio_is_ready_dt(&btn_power))
	{
		LOG_ERR("Error: Button %s is not ready", btn_power.port->name);
		return -1;
	}
	if (!gpio_is_ready_dt(&btn_start))
	{
		LOG_ERR("Error: Button %s is not ready", btn_start.port->name);
		return -1;
	}

	ret = gpio_pin_configure_dt(&led_power, GPIO_OUTPUT_ACTIVE);
	if (ret < 0)
	{
		LOG_ERR("Failed to configure Power LED");
		return ret;
	}
	ret = gpio_pin_configure_dt(&led_status, GPIO_OUTPUT_ACTIVE);
	if (ret < 0)
	{
		LOG_ERR("Failed to configure Status LED");
		return ret;
	}
	ret = gpio_pin_configure_dt(&btn_power, GPIO_INPUT);
	if (ret != 0)
	{
		LOG_ERR("Error %d: failed to configure %s pin %d", ret, btn_power.port->name, btn_power.pin);
		return ret;
	}
	ret = gpio_pin_configure_dt(&btn_start, GPIO_INPUT);
	if (ret != 0)
	{
		LOG_ERR("Error %d: failed to configure %s pin %d", ret, btn_start.port->name, btn_start.pin);
		return ret;
	}

	ret = gpio_pin_set_dt(&led_power, 0);
	if (ret < 0)
	{
		LOG_ERR("Error setting Power LED");
		return ret;
	}
	ret = gpio_pin_set_dt(&led_status, 0);
	if (ret < 0)
	{
		LOG_ERR("Error setting Power LED");
		return ret;
	}

	return 0;
}

void handle_button_press(enum button_id btn)
{
	switch (btn)
	{
	case BTN_POWER:
		LOG_INF("Handling BTN_POWER_PRESSED event...");
		if (gpio_pin_toggle_dt(&led_power) < 0)
		{
			LOG_ERR("Err toggling Power LED");
		}
		break;
	case BTN_START:
		LOG_INF("Handling BTN_START_PRESSED event...");
		if (gpio_pin_toggle_dt(&led_status) < 0)
		{
			LOG_ERR("Err toggling Status LED");
		}
		is_measuring = !is_measuring;
		if (is_measuring)
		{
			LOG_INF("Starting measurement...");
			struct ctrl_event cmd = {.cmd = CMD_TEMP_READ};
			k_msgq_put(&ctrl_msgq, &cmd, K_FOREVER);
		}
		else
		{
			LOG_INF("Measurement stopped");
		}
		break;
	default:
		break;
	}
}

void handle_event(struct app_event *data)
{
	switch (data->event)
	{
	case BTN_PRESSED:
		handle_button_press(data->data.button.id);
		break;
	case TEMP_READY:
		LOG_INF("Temperature ready - is %0.1lf°C", data->data.temp.temperature);
		break;
	default:
		break;
	}
}

void handle_cmd(struct ctrl_event *data)
{
	switch (data->cmd)
	{
	case CMD_TEMP_READ:
		LOG_INF("Handling CMD_TEMP_READ command...");
		double value;
		int ret = temp_read(dev_temp0, &value);
		if (ret != 0)
		{
			LOG_ERR("Failed to read temperature: %d", ret);
			break;
		}
		struct app_event evt =
			{
				.event = TEMP_READY,
				.data.temp.temperature = value,
			};
		k_msgq_put(&app_msgq, &evt, K_FOREVER);
		break;
	default:
		break;
	}
}

void app_state_thread(void *, void *, void *)
{
	LOG_INF("AppState thread running...");
	struct app_event data;
	int ret;
	while (1)
	{
		ret = k_msgq_get(&app_msgq, &data, K_FOREVER);
		if (ret < 0)
		{
			LOG_ERR("Error getting event from queue");
			continue;
		}
		handle_event(&data);
	}
}

void temp_thread(void *, void *, void *)
{
	LOG_INF("MeasureTemp thread running...");
	int ret;
	struct ctrl_event data;
	while (1)
	{
		ret = k_msgq_get(&ctrl_msgq, &data, K_FOREVER);
		if (ret < 0)
		{
			LOG_ERR("Error getting from ctrl_msgq");
			continue;
		}
		handle_cmd(&data);
	}
}

int main(void)
{
	LOG_INF("Meatometer - v%s - arch: %s", APP_VERSION_STRING, CONFIG_ARCH);

	int ret;

	ret = init_hardware();
	if (ret < 0)
	{
		return 0;
	}

	// Configure interrupts for buttons
	ret = gpio_pin_interrupt_configure_dt(&btn_power, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0)
	{
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d", ret, btn_power.port->name, btn_power.pin);
		return 0;
	}
	ret = gpio_pin_interrupt_configure_dt(&btn_start, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0)
	{
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d", ret, btn_power.port->name, btn_power.pin);
		return 0;
	}
	gpio_init_callback(&button_cb_data, button_pressed, BIT(btn_power.pin) | BIT(btn_start.pin));
	gpio_add_callback(btn_power.port, &button_cb_data);
	gpio_add_callback(btn_start.port, &button_cb_data);

	if (!device_is_ready(dev_temp0))
	{
		LOG_ERR("Temperature sensor (device %s) is not ready", dev_temp0->name);
		return 0;
	}

	// k_sleep(K_FOREVER);

	return 0;
}

K_THREAD_DEFINE(AppState, TH_STACKSIZE, app_state_thread, NULL, NULL, NULL, TH_APPSTATE_PRIORITY, 0, 0);
K_THREAD_DEFINE(MeasureTemp, TH_STACKSIZE, temp_thread, NULL, NULL, NULL, TH_TEMP_PRIORITY, 0, 0);