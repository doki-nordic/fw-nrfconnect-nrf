/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
#include <nrf.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/shell/shell.h>
#include <dk_buttons_and_leds.h>
#include <hal/nrf_gpio.h>

LOG_MODULE_REGISTER(esb_prx, CONFIG_IRQ_LATENCY_APP_LOG_LEVEL);


#define TRIGGER_NODE DT_NODELABEL(trigger_pin)
#define RESPONSE0_NODE DT_NODELABEL(response0_pin)
#define RESPONSE1_NODE DT_NODELABEL(response1_pin)
#define RESPONSE2_NODE DT_NODELABEL(response2_pin)
#define RESPONSE3_NODE DT_NODELABEL(response3_pin)
#define INPUT_NODE DT_NODELABEL(input_pin)

static const struct gpio_dt_spec trigger_spec = GPIO_DT_SPEC_GET(TRIGGER_NODE, gpios);
static const struct gpio_dt_spec response0_spec = GPIO_DT_SPEC_GET(RESPONSE0_NODE, gpios);
static const struct gpio_dt_spec response1_spec = GPIO_DT_SPEC_GET(RESPONSE1_NODE, gpios);
static const struct gpio_dt_spec response2_spec = GPIO_DT_SPEC_GET(RESPONSE2_NODE, gpios);
static const struct gpio_dt_spec response3_spec = GPIO_DT_SPEC_GET(RESPONSE3_NODE, gpios);
static const struct gpio_dt_spec input_spec = GPIO_DT_SPEC_GET(INPUT_NODE, gpios);

static int test_gpio(const struct shell *shell, size_t argc,
			       char **argv)
{
	static int i = 1;
	nrf_gpio_port_out_clear(NRF_P0_S, 1 << (4 + 0));
	nrf_gpio_port_out_clear(NRF_P0_S, 1 << (4 + 1));
	nrf_gpio_port_out_clear(NRF_P0_S, 1 << (4 + 2));
	nrf_gpio_port_out_clear(NRF_P0_S, 1 << (4 + 3));
	gpio_pin_set_dt(&trigger_spec, i & 1);
	i++;
	return 0;
}

SHELL_CMD_REGISTER(gpio, NULL, "Trigger GPIO test.", test_gpio);

static int test_read(const struct shell *shell, size_t argc, char **argv)
{
	int value = gpio_pin_get_dt(&input_spec);
	shell_fprintf(shell, SHELL_NORMAL, "Value %d\n", value);
	return 0;
}

SHELL_CMD_REGISTER(read, NULL, "Get.", test_read);

static void interrupt_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	//nrf_gpio_port_out_set(NRF_P0_S, 1 << (4 + 3));
	*(int32_t*)(0x5F938000 + 0x004) = 1 << (4 + 3);
}

int main(void)
{
	int err;
	static struct gpio_callback gpio_cb_data;

	LOG_INF("IRQ Latency sample start.");

	err = dk_leds_init();
	if (err) {
		LOG_ERR("LEDs initialization failed, err %d", err);
		return 0;
	}

	err = gpio_pin_configure_dt(&trigger_spec, GPIO_OUTPUT_INACTIVE);
	err = err || gpio_pin_configure_dt(&response0_spec, GPIO_OUTPUT_INACTIVE);
	err = err || gpio_pin_configure_dt(&response1_spec, GPIO_OUTPUT_INACTIVE);
	err = err || gpio_pin_configure_dt(&response2_spec, GPIO_OUTPUT_INACTIVE);
	err = err || gpio_pin_configure_dt(&response3_spec, GPIO_OUTPUT_INACTIVE);
	err = err || gpio_pin_configure_dt(&input_spec, GPIO_INPUT);
	err = err || gpio_pin_interrupt_configure_dt(&input_spec, GPIO_INT_EDGE_BOTH);
	if (err) {
		return -ENODEV;
	}

	gpio_init_callback(&gpio_cb_data, interrupt_callback, BIT(input_spec.pin));
	gpio_add_callback(input_spec.port, &gpio_cb_data);

	LOG_INF("Initialization complete");

	/* return to idle thread */
	return 0;
}
