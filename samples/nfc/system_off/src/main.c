/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <zephyr.h>
#include <power/power.h>

#include <nrfx.h>
#include <hal/nrf_power.h>

#include <nfc_t2t_lib.h>
#include <nfc/ndef/nfc_ndef_msg.h>
#include <nfc/ndef/nfc_text_rec.h>

#include <dk_buttons_and_leds.h>


#define SYSTEM_OFF_DELAY_S	3

#define MAX_REC_COUNT		1
#define NDEF_MSG_BUF_SIZE	128

#define NFC_FIELD_LED		DK_LED1
#define SYSTEM_ON_LED		DK_LED2


/* Delayed work that enters system off. */
static struct k_delayed_work system_off_work;


/**
 * @brief Function that receives events from NFC.
 */
static void nfc_callback(void *context,
			 enum nfc_t2t_event event,
			 const u8_t *data,
			 size_t data_length)
{
	ARG_UNUSED(context);
	ARG_UNUSED(data);
	ARG_UNUSED(data_length);

	switch (event) {
	case NFC_T2T_EVENT_FIELD_ON:
		/* Cancel entering system off */
		k_delayed_work_cancel(&system_off_work);
		dk_set_led_on(NFC_FIELD_LED);
		break;
	case NFC_T2T_EVENT_FIELD_OFF:
		/* Enter system off after delay */
		k_delayed_work_submit(&system_off_work,
				K_SECONDS(SYSTEM_OFF_DELAY_S));
		dk_set_led_off(NFC_FIELD_LED);
		break;
	default:
		break;
	}
}


/**
 * @brief Function for configuring and starting the NFC.
 */
static int start_nfc()
{
	/* Text message in its language code. */
	static const u8_t en_payload[] = {
		'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'
	};
	static const u8_t en_code[] = {'e', 'n'};

	/* Buffer used to hold an NFC NDEF message. */
	static u8_t buffer[NDEF_MSG_BUF_SIZE];

	NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_text_rec,
				      UTF_8,
				      en_code,
				      sizeof(en_code),
				      en_payload,
				      sizeof(en_payload));

	NFC_NDEF_MSG_DEF(nfc_text_msg, MAX_REC_COUNT);

	u32_t len = sizeof(buffer);

	/* Set up NFC */
	if (nfc_t2t_setup(nfc_callback, NULL) < 0) {
		printk("Cannot setup NFC T2T library!\n");
		return -1;
	}

	/* Add record */
	if (nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_text_msg),
				&NFC_NDEF_TEXT_RECORD_DESC(nfc_text_rec)) < 0) {
		printk("Cannot add record!\n");
		return -1;
	}

	/* Encode welcome message */
	if (nfc_ndef_msg_encode(&NFC_NDEF_MSG(nfc_text_msg), buffer, &len) < 0) {
		printk("Cannot encode message!\n");
		return -1;
	}

	/* Set created message as the NFC payload */
	if (nfc_t2t_payload_set(buffer, len) < 0) {
		printk("Cannot set payload!\n");
		return -1;
	}

	/* Start sensing NFC field */
	if (nfc_t2t_emulation_start() < 0) {
		printk("Cannot start emulation!\n");
		return -1;
	}

	printk("NFC configuration done\n");
	return 0;
}


/**
 * @brief Function entering system off.
 * System off is delayed to make sure that NFC tag was correctly read.
 */
static void system_off(struct k_work *work)
{
	printk("Entering system off.\nApproach a NFC reader to restart.\n");

	/* Before we disabled entry to deep sleep. Here we need to override
	 * that, then force a sleep so that the deep sleep takes effect.
	 */
	sys_pm_force_power_state(SYS_POWER_STATE_DEEP_SLEEP_1);
	dk_set_led_off(SYSTEM_ON_LED);
	k_sleep(K_MSEC(1));
	dk_set_led_on(SYSTEM_ON_LED);

	/* Below line will never be executed if system off was correct. */
	printk("ERROR: System off failed\n");
}


void main(void)
{
	/* Configure LED-pins */
	if (dk_leds_init() < 0) {
		printk("Cannot init LEDs!\n");
		return;
	}
	dk_set_led_on(SYSTEM_ON_LED);

	/* Configure and start delayed work that enters system off */
	k_delayed_work_init(&system_off_work, system_off);
	k_delayed_work_submit(&system_off_work, K_SECONDS(SYSTEM_OFF_DELAY_S));

	/* Show last reset reason */
	u32_t reas = nrf_power_resetreas_get(NRF_POWER);
	nrf_power_resetreas_clear(NRF_POWER, reas);
	if (reas & NRF_POWER_RESETREAS_NFC_MASK) {
		printk("Wake up by NFC field detect\n");
	} else if (reas & NRF_POWER_RESETREAS_RESETPIN_MASK) {
		printk("Reset by pin-reset\n");
	} else if (reas & NRF_POWER_RESETREAS_SREQ_MASK) {
		printk("Reset by soft-reset\n");
	} else if (reas) {
		printk("Reset by a different source (0x%08X)\n", reas);
	} else {
		printk("Power-on-reset\n");
	}

	/* Start NFC */
	if (start_nfc() < 0) {
		printk("ERROR: NFC configuration failed\n");
		return;
	}

	/* Prevent deep sleep (system off) from being entered */
	sys_pm_ctrl_disable_state(SYS_POWER_STATE_DEEP_SLEEP_1);

	/* Exit main function - rest will be done by the callbacks */
}
