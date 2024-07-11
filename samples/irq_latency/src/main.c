/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
#include <nrf.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/shell/shell.h>
#include <dk_buttons_and_leds.h>
#include <hal/nrf_gpio.h>
#include <zephyr/drivers/uart.h>

LOG_MODULE_REGISTER(esb_prx, CONFIG_IRQ_LATENCY_APP_LOG_LEVEL);

// 1 - Trigger thread from callback, 0 - Schedule work in system work queue from callback
#define TRIGGER_THREAD 1


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

static const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(ncs_irq_timing));

static const struct pwm_dt_spec pwm = PWM_DT_SPEC_GET(DT_PATH(zephyr_user));

static void triggered_work_handler(struct k_work *item);
static void triggered_thread_entry(void *, void *, void *);

static K_THREAD_DEFINE(triggered_thread, 512,
                triggered_thread_entry, NULL, NULL, NULL,
                K_HIGHEST_APPLICATION_THREAD_PRIO, 0, 0);
static K_WORK_DEFINE(triggered_work, triggered_work_handler);
static K_SEM_DEFINE(triggered_sem, 0, 1);


static int test_gpio(const struct shell *shell, size_t argc,
			       char **argv)
{
	static int i = 0;
	if (i) {
		*(volatile int32_t*)(0x5F938000 + 0x004) = 1 << 0; // P0.0 manual trigger on
	} else {
		*(volatile int32_t*)(0x5F938000 + 0x008) = 1 << 0; // P0.0 manual trigger off
	}
	i = !i;
	return 0;
}

SHELL_CMD_REGISTER(gpio, NULL, "Toggle P0.0 pin manually.", test_gpio);


static int cmd_awake(const struct shell *shell, size_t argc, char **argv)
{
	int64_t start = k_uptime_get();
	while (start + 5000 > k_uptime_get()) {
		for (int i = 0; i < 1000000; i++) {
			__asm volatile("");
		}
	}
	return 0;
}

SHELL_CMD_REGISTER(awake, NULL, "Force CPU awake for 5 sec.", cmd_awake);


static int cmd_tx(const struct shell *shell, size_t argc, char **argv)
{
	static char src[] = "0";
	int64_t start = k_uptime_get();
	while (start + 2000 > k_uptime_get());
	uart_tx(uart, src, 1, SYS_FOREVER_US);
	src[0]++;
	return 0;
}

SHELL_CMD_REGISTER(tx, NULL, "Send byte on UARTE TX on P1.4 pin.", cmd_tx);


#define REPEAT4(x) x x x x
#define REPEAT16(x) REPEAT4(x) REPEAT4(x) REPEAT4(x) REPEAT4(x)
#define REPEAT64(x) REPEAT16(x) REPEAT16(x) REPEAT16(x) REPEAT16(x)
#define REPEAT256(x) REPEAT64(x) REPEAT64(x) REPEAT64(x) REPEAT64(x)
#define REPEAT1024(x) REPEAT256(x) REPEAT256(x) REPEAT256(x) REPEAT256(x)
#define REPEAT4096(x) REPEAT1024(x) REPEAT1024(x) REPEAT1024(x) REPEAT1024(x)

//#define WAIT for (volatile int c = 0; c < 10; c++);
//#define WAIT __DSB()
//#define WAIT __DMB()
#define WAIT

#define USE_VALUE(x) __asm__ volatile (""::"r"(x));

static int cmd_regw(const struct shell *shell, size_t argc, char **argv)
{
	for (int i = 0; i < 100; i++) {
		*(volatile int32_t*)(0x5F938000 + 0x008) = 1 << 6; // P0.6 off
		WAIT;
		*(volatile int32_t*)(0x5F939200 + 0x008) = 1 << 0; // P9.0 off
		WAIT;
		*(volatile int32_t*)(0x5F938000 + 0x004) = 1 << 6; // P0.6 on
		WAIT;
		*(volatile int32_t*)(0x5F939200 + 0x004) = 1 << 0; // P9.0 on
		WAIT;
	}
	return 0;
}

SHELL_CMD_REGISTER(regw, NULL, "Test GPIO registers write time.", cmd_regw);

static int cmd_regw_uarte(const struct shell *shell, size_t argc, char **argv)
{
	uint32_t value = *(volatile int32_t*)(0x5F9C5000 + 0x524);
	for (int i = 0; i < 100; i++) {
		*(volatile int32_t*)(0x5F938000 + 0x008) = 1 << 6; // P0.6 off
		WAIT;
		*(volatile int32_t*)(0x5F9C5000 + 0x524) = value;
		WAIT;
		*(volatile int32_t*)(0x5F9C5000 + 0x524) = value;
		WAIT;
		*(volatile int32_t*)(0x5F9C5000 + 0x524) = value;
		WAIT;
		*(volatile int32_t*)(0x5F9C5000 + 0x524) = value;
		WAIT;
		*(volatile int32_t*)(0x5F938000 + 0x004) = 1 << 6; // P0.6 on
		WAIT;
	}
	return 0;
}

SHELL_CMD_REGISTER(regw_uarte, NULL, "Test UARTE registers write time.", cmd_regw_uarte);

static int cmd_regr(const struct shell *shell, size_t argc, char **argv)
{
	uint32_t x;
	for (int i = 0; i < 100; i++) {
		*(volatile int32_t*)(0x5F938000 + 0x008) = 1 << 6; // P0.6 off
		WAIT;
		x = *(volatile int32_t*)(0x5F938000 + 0x00C); USE_VALUE(x);
		WAIT;
		x = *(volatile int32_t*)(0x5F938200 + 0x00C); USE_VALUE(x);
		WAIT;
		x = *(volatile int32_t*)(0x5F938400 + 0x00C); USE_VALUE(x);
		WAIT;
		x = *(volatile int32_t*)(0x5F938C00 + 0x00C); USE_VALUE(x);
		WAIT;
		*(volatile int32_t*)(0x5F938000 + 0x004) = 1 << 6; // P0.6 on
		WAIT;
	}
	return 0;
}

SHELL_CMD_REGISTER(regr, NULL, "Test GPIO registers read time.", cmd_regr);

static int cmd_regr_uart(const struct shell *shell, size_t argc, char **argv)
{
	uint32_t x;
	for (int i = 0; i < 100; i++) {
		*(volatile int32_t*)(0x5F938000 + 0x008) = 1 << 6; // P0.6 off
		WAIT;
		x = *(volatile int32_t*)(0x5F9C5000 + 0x524); USE_VALUE(x);
		WAIT;
		x = *(volatile int32_t*)(0x5F9C5000 + 0x56C); USE_VALUE(x);
		WAIT;
		x = *(volatile int32_t*)(0x5F9C5000 + 0x524); USE_VALUE(x);
		WAIT;
		x = *(volatile int32_t*)(0x5F9C5000 + 0x56C); USE_VALUE(x);
		WAIT;
		x = *(volatile int32_t*)(0x5F9C5000 + 0x524); USE_VALUE(x);
		WAIT;
		x = *(volatile int32_t*)(0x5F9C5000 + 0x56C); USE_VALUE(x);
		WAIT;
		x = *(volatile int32_t*)(0x5F9C5000 + 0x524); USE_VALUE(x);
		WAIT;
		x = *(volatile int32_t*)(0x5F9C5000 + 0x56C); USE_VALUE(x);
		WAIT;
		*(volatile int32_t*)(0x5F938000 + 0x004) = 1 << 6; // P0.6 on
		WAIT;
	}
	return 0;
}

SHELL_CMD_REGISTER(regr_uarte, NULL, "Test UARTE registers read access time.", cmd_regr_uart);


static void interrupt_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	*(volatile int32_t*)(0x5F938000 + 0x004) = 1 << 7; // P0.7 on - start callback
	if (TRIGGER_THREAD) {
		k_sem_give(&triggered_sem);
	} else {
		k_work_submit(&triggered_work);
	}
	*(volatile int32_t*)(0x5F938000 + 0x008) = 1 << 7; // P0.7 off - end callback
}


static uint8_t uart_buffer[3][512];
static uint8_t uart_buffer_used;


static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	ARG_UNUSED(dev);
	*(volatile int32_t*)(0x5F938000 + 0x004) = 1 << 7; // P0.7 on - start callback

	//LOG_ERR("Callback %d", evt->type);

	switch (evt->type) {
	case UART_TX_DONE:
		break;

	case UART_RX_RDY:
		*(volatile int32_t*)(0x5F938000 + 0x008) = 1 << 6; // P0.6 off
		*(volatile int32_t*)(0x5F938000 + 0x004) = 1 << 6; // P0.6 on
		break;

	case UART_RX_DISABLED:
		if (!(uart_buffer_used & 1)) {
			uart_buffer_used |= 1;
			uart_rx_enable(uart, uart_buffer[0], sizeof(uart_buffer[0]), 0);
		} else if (!(uart_buffer_used & 2)) {
			uart_buffer_used |= 2;
			uart_rx_enable(uart, uart_buffer[1], sizeof(uart_buffer[1]), 0);
		} else if (!(uart_buffer_used & 4)) {
			uart_buffer_used |= 4;
			uart_rx_enable(uart, uart_buffer[2], sizeof(uart_buffer[2]), 0);
		}
		break;

	case UART_RX_BUF_REQUEST:
		if (!(uart_buffer_used & 1)) {
			uart_buffer_used |= 1;
			uart_rx_buf_rsp(uart, uart_buffer[0], sizeof(uart_buffer[0]));
		} else if (!(uart_buffer_used & 2)) {
			uart_buffer_used |= 2;
			uart_rx_buf_rsp(uart, uart_buffer[1], sizeof(uart_buffer[1]));
		} else if (!(uart_buffer_used & 4)) {
			uart_buffer_used |= 4;
			uart_rx_buf_rsp(uart, uart_buffer[2], sizeof(uart_buffer[2]));
		}
		break;

	case UART_RX_BUF_RELEASED:
		if (evt->data.rx_buf.buf == uart_buffer[0]) {
			uart_buffer_used &= ~1;
		} else if (evt->data.rx_buf.buf == uart_buffer[1]) {
			uart_buffer_used &= ~2;
		} else if (evt->data.rx_buf.buf == uart_buffer[2]) {
			uart_buffer_used &= ~4;
		}
		break;

	case UART_TX_ABORTED:
		break;

	default:
		break;
	}

	*(volatile int32_t*)(0x5F938000 + 0x008) = 1 << 7; // P0.7 off - end callback
}

static void trigger_uart() {
	static char buf[] = "1";
	uart_tx(uart, buf, 1, SYS_FOREVER_US);
	buf[0]++;
}

static void triggered_work_handler(struct k_work *item) {
	*(volatile int32_t*)(0x5F938000 + 0x008) = 1 << 6; // P0.6 off
	*(volatile int32_t*)(0x5F938000 + 0x004) = 1 << 6; // P0.6 on
	trigger_uart();
}

static void triggered_thread_entry(void *, void *, void *) {
	while (1) {
		k_sem_take(&triggered_sem, K_FOREVER);
		*(volatile int32_t*)(0x5F938000 + 0x008) = 1 << 6; // P0.6 off
		*(volatile int32_t*)(0x5F938000 + 0x004) = 1 << 6; // P0.6 on
		trigger_uart();
	}
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

	if (!pwm_is_ready_dt(&pwm)) {
		printk("Error: PWM device %s is not ready\n", pwm.dev->name);
		return 0;
	}

	if (pwm_set_dt(&pwm, PWM_MSEC(100), PWM_USEC(9))) { // 78 to emulate 115200, 9 for 1M, 36 for 250K
		LOG_ERR("Pwm led 4 set fails:\n");
		return 0;
	}

	err = gpio_pin_configure_dt(&trigger_spec, GPIO_OUTPUT_INACTIVE);
	err = err || gpio_pin_configure_dt(&response0_spec, GPIO_OUTPUT_INACTIVE);
	err = err || gpio_pin_configure_dt(&response1_spec, GPIO_OUTPUT_INACTIVE);
	err = err || gpio_pin_configure_dt(&response2_spec, GPIO_OUTPUT_INACTIVE);
	err = err || gpio_pin_configure_dt(&response3_spec, GPIO_OUTPUT_INACTIVE);
	err = err || gpio_pin_configure_dt(&input_spec, GPIO_INPUT);
	err = err || gpio_pin_interrupt_configure_dt(&input_spec, GPIO_INT_EDGE_FALLING);
	if (err) {
		return -ENODEV;
	}

	gpio_init_callback(&gpio_cb_data, interrupt_callback, BIT(input_spec.pin));
	gpio_add_callback(input_spec.port, &gpio_cb_data);

	if (!device_is_ready(uart)) {
		LOG_ERR("UART device not ready");
		return -ENODEV;
	}

	err = uart_callback_set(uart, uart_cb, NULL);
	if (err) {
		LOG_ERR("UART callback setup failed %d", err);
		return err;
	}

	err = uart_rx_enable(uart, uart_buffer[0], sizeof(uart_buffer[0]), 5000);
	uart_buffer_used |= 1;
	if (err) {
		LOG_ERR("UART enable failed %d", err);
		return err;
	}

	LOG_INF("Initialization complete");

	/* return to idle thread */
	return 0;
}
