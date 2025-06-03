/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <hal/nrf_gpio.h>
#include <app_event_manager.h>

#define MODULE main
#include <caf/events/module_state_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE);


/*#define DEBUG0_NODE DT_NODELABEL(debug0pin)
#define DEBUG1_NODE DT_NODELABEL(debug1pin)
#define DEBUG2_NODE DT_NODELABEL(debug2pin)
#define DEBUG3_NODE DT_NODELABEL(debug3pin)

const struct gpio_dt_spec debug0_spec = GPIO_DT_SPEC_GET(DEBUG0_NODE, gpios);
const struct gpio_dt_spec debug1_spec = GPIO_DT_SPEC_GET(DEBUG1_NODE, gpios);
const struct gpio_dt_spec debug2_spec = GPIO_DT_SPEC_GET(DEBUG2_NODE, gpios);
const struct gpio_dt_spec debug3_spec = GPIO_DT_SPEC_GET(DEBUG3_NODE, gpios);*/

/*
bool started = false;

void _debug_pin(int n, int value)
{
	if (!started) return;
	switch (n) {
	case 1:
		gpio_pin_set_dt(&debug0_spec, value);
		break;
	case 3:
		gpio_pin_set_dt(&debug1_spec, value);
		break;
	case 5:
		gpio_pin_set_dt(&debug2_spec, value);
		break;
	case 7:
		gpio_pin_set_dt(&debug7_spec, value);
		break;
	}
}*/

int main(void)
{
	// int err = 0;
	// err = err || gpio_pin_configure_dt(&debug0_spec, GPIO_OUTPUT_INACTIVE);
	// err = err || gpio_pin_configure_dt(&debug1_spec, GPIO_OUTPUT_INACTIVE);
	// err = err || gpio_pin_configure_dt(&debug2_spec, GPIO_OUTPUT_INACTIVE);
	// err = err || gpio_pin_configure_dt(&debug3_spec, GPIO_OUTPUT_INACTIVE);
	// if (err) {
	// 	LOG_INF("Machine learning: GPIO init failed");
	// 	return 1;
	// }
	//started = true;

	// *(volatile uint32_t *)(0x5F938000 + 8) = 0xF;
	// k_sleep(K_MSEC(50));
	// *(volatile uint32_t *)(0x5F938000 + 4) = 0xF;
	// k_sleep(K_MSEC(50));
	// *(volatile uint32_t *)(0x5F938000 + 8) = 0xF;
	// k_sleep(K_MSEC(50));
	// *(volatile uint32_t *)(0x5F938000 + 4) = 0xF;
	// k_sleep(K_MSEC(50));
	// *(volatile uint32_t *)(0x5F938000 + 8) = 0xF;
	// k_sleep(K_MSEC(50));
	// *(volatile uint32_t *)(0x5F938000 + 4) = 0xF;

	while (1) k_sleep(K_MSEC(10000));

	if (app_event_manager_init()) {
		LOG_ERR("Application Event Manager initialization failed");
	} else {
		module_set_state(MODULE_STATE_READY);
	}
	return 0;
}
