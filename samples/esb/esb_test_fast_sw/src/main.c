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
#if CONFIG_SOC_NRF52840
#include <nrfx_ppi.h>
#else
#include <nrfx_dppi.h>
#endif
#include <nrfx_timer.h>
#include <hal/nrf_egu.h>
#include <hal/nrf_radio.h>
#include <hal/nrf_timer.h>
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

#if defined(CONFIG_CLOCK_CONTROL_NRF)
int clocks_start(void)
{
	int err;
	int res;
	struct onoff_manager *clk_mgr;
	struct onoff_client clk_cli;

	clk_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
	if (!clk_mgr) {
		LOG_ERR("Unable to get the Clock manager");
		return -ENXIO;
	}

	sys_notify_init_spinwait(&clk_cli.notify);

	err = onoff_request(clk_mgr, &clk_cli);
	if (err < 0) {
		LOG_ERR("Clock request failed: %d", err);
		return err;
	}

	do {
		err = sys_notify_fetch_result(&clk_cli.notify, &res);
		if (!err && res) {
			LOG_ERR("Clock could not be started: %d", res);
			return res;
		}
	} while (err);

#if NRF54L_ERRATA_20_PRESENT
	if (nrf54l_errata_20()) {
		nrf_power_task_trigger(NRF_POWER, NRF_POWER_TASK_CONSTLAT);
	}
#endif /* NRF54L_ERRATA_20_PRESENT */

#if defined(NRF54LM20A_ENGA_XXAA)
	/* MLTPAN-39 */
	nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_PLLSTART);
#endif

	LOG_DBG("HF clock started");
	return 0;
}

#elif defined(CONFIG_CLOCK_CONTROL_NRF2)

int clocks_start(void)
{
	int err;
	int res;
	const struct device *radio_clk_dev =
		DEVICE_DT_GET_OR_NULL(DT_CLOCKS_CTLR(DT_NODELABEL(radio)));
	struct onoff_client radio_cli;

	/** Keep radio domain powered all the time to reduce latency. */
	nrf_lrcconf_poweron_force_set(NRF_LRCCONF010, NRF_LRCCONF_POWER_DOMAIN_1, true);

	sys_notify_init_spinwait(&radio_cli.notify);

	err = nrf_clock_control_request(radio_clk_dev, NULL, &radio_cli);

	do {
		err = sys_notify_fetch_result(&radio_cli.notify, &res);
		if (!err && res) {
			LOG_ERR("Clock could not be started: %d", res);
			return res;
		}
	} while (err == -EAGAIN);

	nrf_lrcconf_clock_always_run_force_set(NRF_LRCCONF000, 0, true);
	nrf_lrcconf_task_trigger(NRF_LRCCONF000, NRF_LRCCONF_TASK_CLKSTART_0);

	LOG_DBG("HF clock started");
	return 0;
}

#else
BUILD_ASSERT(false, "No Clock Control driver");
#endif /* defined(CONFIG_CLOCK_CONTROL_NRF2) */

const char buffer[256] = {0};

static const uint32_t FREQUENCY = 2408;
static const uint32_t BASE_ADDR = 0x63e0;
static const uint32_t PREFIX_BYTE_ADDR = 0x17;
static const uint32_t CRC_POLY = 0x864CFB; // CRC-24-Radix-64 (OpenPGP)

void setup_radio()
{

	int i = clocks_start();
	if (i != 0) {
		LOG_ERR("Failed to start clocks");
		while(1);
	}

	NRF_RADIO->TXPOWER = 1;
	#if IS_ENABLED(CONFIG_SOC_NRF52840) || IS_ENABLED(CONFIG_SOC_NRF5340_CPUNET)
	NRF_RADIO->INTENCLR = 0xFFFFFFFF;
	#else
	NRF_RADIO->INTENCLR00 = 0xFFFFFFFF;
	#endif
	NRF_RADIO->PACKETPTR = (uint32_t)&buffer[0];
	NRF_RADIO->FREQUENCY = FREQUENCY - 2400;
	NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_1Mbit;
	NRF_RADIO->PCNF0 =
		(0 << RADIO_PCNF0_LFLEN_Pos) |
		(0 << RADIO_PCNF0_S0LEN_Pos) |
		(0 << RADIO_PCNF0_S1LEN_Pos);
	NRF_RADIO->PCNF1 =
		(10 << RADIO_PCNF1_MAXLEN_Pos) |
		(10 << RADIO_PCNF1_STATLEN_Pos) |
		(2 << RADIO_PCNF1_BALEN_Pos) |
		(RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos);
	NRF_RADIO->BASE0 = BASE_ADDR;
	NRF_RADIO->PREFIX0 = PREFIX_BYTE_ADDR << RADIO_PREFIX0_AP0_Pos;
	NRF_RADIO->TXADDRESS = 0;
	NRF_RADIO->RXADDRESSES = RADIO_RXADDRESSES_ADDR0_Msk;
	//NRF_RADIO->CRCCNF = RADIO_CRCCNF_LEN_Disabled << RADIO_CRCCNF_LEN_Pos;
	NRF_RADIO->CRCCNF = RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos;
	NRF_RADIO->CRCPOLY = CRC_POLY;
	NRF_RADIO->CRCINIT = 0;
	#if CONFIG_SOC_NRF5340_CPUNET
	NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_0dBm;
	#else 
	NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos4dBm;
	#endif
// 	NRF_RADIO->POWER = 1;
// 	NRF_RADIO->PACKETPTR = (uint32_t)buffer;
// 	NRF_RADIO->FREQUENCY = 7; // 2407 MHz
// 	NRF_RADIO->MODE = 0;
// 	NRF_RADIO->PCNF0 = (1 << RADIO_PCNF0_PLEN_Pos);
// 	NRF_RADIO->PCNF1 = (4 << RADIO_PCNF1_STATLEN_Pos) | (4 << RADIO_PCNF1_MAXLEN_Pos) | (2 << RADIO_PCNF1_BALEN_Pos);
// 	NRF_RADIO->RXADDRESSES = 0xF;
// 	NRF_RADIO->BASE0 = 0x89BED600;
// 	NRF_RADIO->BASE1 = 0x89BED600;
// 	NRF_RADIO->PREFIX0 = 0x11111111;
// 	NRF_RADIO->PREFIX1 = 0x11111111;
}

#if 0 // CONFIG_SYS_HEAP_ALLOC_LOOPS!=4

int main(void)
{
	setup_radio();
	LOG_INF("Initialization complete");

	NRF_RADIO->EVENTS_READY = 0;
	NRF_RADIO->TASKS_TXEN = 1;
	while (!(NRF_RADIO->EVENTS_READY));
	LOG_INF("TXEN complete");

	while (1) {
		NRF_RADIO->EVENTS_END = 0;
		NRF_RADIO->TASKS_START = 1;
		while (!(NRF_RADIO->EVENTS_END));
		LOG_INF("TASKS_START complete");
		k_sleep(K_MSEC(100));
	}
}

#else 

#if CONFIG_SOC_NRF54L15_CPUAPP
static nrfx_dppi_t dppi = NRFX_DPPI_INSTANCE(10);
static nrfx_timer_t timer = NRFX_TIMER_INSTANCE(10);
#elif CONFIG_SOC_NRF5340_CPUNET
static nrfx_dppi_t dppi = NRFX_DPPI_INSTANCE(0);
static nrfx_timer_t timer = NRFX_TIMER_INSTANCE(2);
#elif CONFIG_SOC_NRF52840
static nrfx_timer_t timer = NRFX_TIMER_INSTANCE(3);
#else
#error "Not implemented for this SoC"
#endif


int main(void)
{
	int64_t start;
	int64_t time;
	uint8_t channel1;
	uint8_t channel2;
	int i, j, k, err;
	setup_radio();
	LOG_INF("Initialization complete");

	NRF_RADIO->EVENTS_READY = 0;
	NRF_RADIO->TASKS_RXEN = 1;
	while (!(NRF_RADIO->EVENTS_READY));
	LOG_INF("RXEN complete");

	timer.p_reg->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;
	timer.p_reg->PRESCALER = 0;
	//timer.p_reg->SUBSCRIBE_CAPTURE[1] = channel1 | TIMER_SUBSCRIBE_CAPTURE_EN_Msk;
	//timer.p_reg->SUBSCRIBE_CAPTURE[2] = channel2 | TIMER_SUBSCRIBE_CAPTURE_EN_Msk;
	//NRF_RADIO->PUBLISH_END = channel1 | RADIO_PUBLISH_END_EN_Msk;
	//NRF_RADIO->PUBLISH_CRCOK = channel2 | RADIO_PUBLISH_CRCOK_EN_Msk;
	//dppi.p_reg->CHENSET = DPPIC_CHENSET_CH1_Msk;
	//dppi.p_reg->CHENSET = DPPIC_CHENSET_CH2_Msk;
	timer.p_reg->TASKS_START = 1;
	timer.p_reg->TASKS_CAPTURE[1] = 1;
	timer.p_reg->TASKS_CAPTURE[2] = 1;

	#if CONFIG_SOC_NRF52840
	err = nrfx_ppi_channel_alloc(&channel1);
	err = nrfx_ppi_channel_alloc(&channel2);
	nrf_ppi_channel_endpoint_setup(NRF_PPI, channel1,
		nrf_radio_event_address_get(NRF_RADIO, NRF_RADIO_EVENT_END),
		nrf_timer_task_address_get(timer.p_reg, NRF_TIMER_TASK_CAPTURE1));
	nrf_ppi_channel_endpoint_setup(NRF_PPI, channel2,
		nrf_radio_event_address_get(NRF_RADIO, NRF_RADIO_EVENT_CRCOK),
		nrf_timer_task_address_get(timer.p_reg, NRF_TIMER_TASK_CAPTURE2));
	nrf_ppi_channels_enable(NRF_PPI, BIT(channel1) | BIT(channel2));
	#else
	err = nrfx_dppi_channel_alloc(&dppi, &channel1);
	err = nrfx_dppi_channel_alloc(&dppi, &channel2);
	nrf_timer_subscribe_set(timer.p_reg, NRF_TIMER_TASK_CAPTURE1, channel1);
	nrf_timer_subscribe_set(timer.p_reg, NRF_TIMER_TASK_CAPTURE2, channel2);
	nrf_radio_publish_set(NRF_RADIO, NRF_RADIO_EVENT_END, channel1);
	nrf_radio_publish_set(NRF_RADIO, NRF_RADIO_EVENT_CRCOK, channel2);
	nrf_dppi_channels_enable(dppi.p_reg, BIT(channel1) | BIT(channel2));
	#endif

	LOG_ERR("gppi_channel_alloc result=%08X, channel=%d", err, channel1);
	LOG_ERR("gppi_channel_alloc result=%08X, channel=%d", err, channel2);

	while (1) {

		/*NRF_RADIO->EVENTS_END = 0;
		NRF_RADIO->EVENTS_CRCOK = 0;
		NRF_RADIO->EVENTS_CRCERROR = 0;
		NRF_RADIO->TASKS_START = 1;
		i = 0;
		while (!(NRF_RADIO->EVENTS_CRCERROR));
		while (!(NRF_RADIO->EVENTS_END)) {
			i++;
		}
		LOG_INF("END event received after %d loops from CRCERROR", i);

		NRF_RADIO->EVENTS_END = 0;
		NRF_RADIO->EVENTS_CRCOK = 0;
		NRF_RADIO->EVENTS_CRCERROR = 0;
		NRF_RADIO->TASKS_START = 1;
		i = 0;
		while (!(NRF_RADIO->EVENTS_END));
		while (!(NRF_RADIO->EVENTS_CRCERROR)) {
			i++;
		}
		LOG_INF("CRCERROR event received after %d loops from END", i);*/

		NRF_RADIO->EVENTS_END = 0;
		NRF_RADIO->EVENTS_PHYEND = 0;
		NRF_RADIO->EVENTS_CRCOK = 0;
		NRF_RADIO->EVENTS_CRCERROR = 0;
		NRF_RADIO->TASKS_START = 1;
		i = 0;
		while (!(NRF_RADIO->EVENTS_END));
		i = NRF_RADIO->EVENTS_CRCOK;
		//LOG_INF("END event received after %d loops from CRCOK", i);

		NRF_RADIO->EVENTS_END = 0;
		NRF_RADIO->EVENTS_PHYEND = 0;
		NRF_RADIO->EVENTS_CRCOK = 0;
		NRF_RADIO->EVENTS_CRCERROR = 0;
		NRF_RADIO->TASKS_START = 1;
		j = 0;
		while (!(NRF_RADIO->EVENTS_CRCOK));
		j = NRF_RADIO->EVENTS_END;
		//LOG_INF("CRCOK event received after %d loops from END", i);
		timer.p_reg->TASKS_CAPTURE[0] = 1;
		k = timer.p_reg->CC[0];
		int cc1 = timer.p_reg->CC[1];
		int cc2 = timer.p_reg->CC[2];
		#define CLOCK_TICKS_PER_US 16
		LOG_INF("%d %d %d %d %d %d ticks = %d ns ± %d ns", i, j, k, cc1, cc2, cc1 - cc2, (cc1 - cc2) * 1000 / CLOCK_TICKS_PER_US, 1000 / CLOCK_TICKS_PER_US);
		//LOG_INF("%d %d", i, j);
	}

	while (1);

	// suspend(100);

	// start = k_uptime_ticks();
	// NRF_RADIO->TASKS_TXEN = 1;
	// while (NRF_RADIO->STATE != 10);
	// time = k_uptime_ticks() - start;
	// LOG_INF("TX ramp up switch time: %lld us (%lld ticks)", (time * 1000000uLL / CONFIG_SYS_CLOCK_TICKS_PER_SEC), time);

	// suspend(100);

	start = k_uptime_ticks();
	NRF_RADIO->TASKS_RXEN = 1;
	while (NRF_RADIO->STATE != 2);
	time = k_uptime_ticks() - start;
	LOG_INF("TX->RX switch time: %lld us (%lld ticks)", (time * 1000000uLL / CONFIG_SYS_CLOCK_TICKS_PER_SEC), time);

	suspend(100);

	start = k_uptime_ticks();
	NRF_RADIO->TASKS_START = 1;
	while (!NRF_RADIO->EVENTS_END) {
		//suspend(100);
	}
	time = k_uptime_ticks() - start;
	LOG_INF("Receive time: %lld us (%lld ticks)", (time * 1000000uLL / CONFIG_SYS_CLOCK_TICKS_PER_SEC), time);

	// start = k_uptime_ticks();
	// NRF_RADIO->TASKS_TXEN = 1;
	// while (NRF_RADIO->STATE != 10);
	// time = k_uptime_ticks() - start;
	// LOG_INF("RX->TX switch time: %lld us (%lld ticks)", (time * 1000000uLL / CONFIG_SYS_CLOCK_TICKS_PER_SEC), time);


	LOG_INF("MAIN LOOP");
	suspend(1000);
	suspend(1000);
	suspend(1000);
	suspend(1000);
	while (1);
}

#endif
