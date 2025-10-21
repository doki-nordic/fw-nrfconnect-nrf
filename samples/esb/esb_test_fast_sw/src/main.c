/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
#include <nrf.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/pm/device_runtime.h>
#include <dk_buttons_and_leds.h>
#if defined(CONFIG_CLOCK_CONTROL_NRF2)
#include <hal/nrf_lrcconf.h>
#endif
#include <nrf_erratas.h>
#if NRF54L_ERRATA_20_PRESENT
#include <hal/nrf_power.h>
#endif /* NRF54L_ERRATA_20_PRESENT */
#if defined(NRF54LM20A_ENGA_XXAA)
#include <hal/nrf_clock.h>
#endif /* defined(NRF54LM20A_ENGA_XXAA) */

LOG_MODULE_REGISTER(esb_ptx, CONFIG_ESB_PTX_APP_LOG_LEVEL);

static const char *const radio_state_str[] = {
	"0 DISABLED",
	"1 RXRU",
	"2 RXIDLE",
	"3 RX",
	"4 RXDISABLE",
	"5 SETTLE",
	"6 PLL",
	"7 unknown",
	"8 unknown",
	"9 TXRU",
	"10 TXIDLE",
	"11 TX",
	"12 TXDISABLE",
};

void suspend(uint32_t ms)
{
	LOG_INF("BEFORE %s", radio_state_str[NRF_RADIO->STATE]);
	k_timeout_t t = K_MSEC(ms);
	k_sleep(t);
	LOG_INF("AFTER  %s", radio_state_str[NRF_RADIO->STATE]);
}

int main(void)
{
	LOG_INF("Initialization complete");

	suspend(100);

	int64_t start = k_uptime_ticks();
	NRF_RADIO->TASKS_TXEN = 1;
	while (NRF_RADIO->STATE != 10);
	int64_t time = k_uptime_ticks() - start;
	LOG_INF("TX ramp up switch time: %lld us (%lld ticks)", (time * 1000000uLL / CONFIG_SYS_CLOCK_TICKS_PER_SEC), time);

	suspend(100);

	start = k_uptime_ticks();
	NRF_RADIO->TASKS_RXEN = 1;
	while (NRF_RADIO->STATE != 2);
	time = k_uptime_ticks() - start;
	LOG_INF("TX->RX switch time: %lld us (%lld ticks)", (time * 1000000uLL / CONFIG_SYS_CLOCK_TICKS_PER_SEC), time);

	suspend(100);

	start = k_uptime_ticks();
	NRF_RADIO->TASKS_TXEN = 1;
	while (NRF_RADIO->STATE != 10);
	time = k_uptime_ticks() - start;
	LOG_INF("RX->TX switch time: %lld us (%lld ticks)", (time * 1000000uLL / CONFIG_SYS_CLOCK_TICKS_PER_SEC), time);

	LOG_INF("MAIN LOOP");
	suspend(1000);
	suspend(1000);
	suspend(1000);
	suspend(1000);
	while (1);
}
