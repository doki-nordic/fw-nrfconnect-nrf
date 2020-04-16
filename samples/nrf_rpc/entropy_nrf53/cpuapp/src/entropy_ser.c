/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <errno.h>
#include <init.h>

#include <nrf_rpc.h>
#include <cbor.h>

#include "../../ser_common.h"

NRF_RPC_GROUP_DEFINE(entropy_group, NRF_RPC_USER_GROUP_FIRST);

rp_err_t rsp_error_code_handle(const uint8_t *packet, size_t len,
				  void* hander_data)
{
	if (len < sizeof(int)) {
		return NRF_RPC_ERR_INVALID_PARAM;
	}
	*(int *)hander_data = *(int *)&packet[0];

	return NRF_RPC_SUCCESS;
}

int entropy_remote_init(void)
{
	int result;
	rp_err_t err;
	uint8_t* packet;

	printk("entropy_remote_init\n");

	NRF_RPC_CMD_ALLOC(&entropy_group, &packet, 0, return -ENOMEM);

	printbuf("entropy_remote_init", packet, 0);

	err = NRF_RPC_CMD_SEND(&entropy_group, SER_COMMAND_ENTROPY_INIT, packet, 0, rsp_error_code_handle, &result);
	if (err) {
		return -EINVAL;
	}

	return result;
}

struct entropy_get_result {
	u8_t *buffer;
	u16_t length;
	int result;
};

static rp_err_t entropy_get_rsp(const uint8_t *packet, size_t len,
				  void* hander_data)
{
	struct entropy_get_result *result = (struct entropy_get_result *)hander_data;

	if (len != sizeof(int) + result->length) {
		return NRF_RPC_ERR_INTERNAL;
	}
	result->result = *(int *)&packet[0];

	memcpy(result->buffer, &packet[sizeof(int)], result->length);

	return NRF_RPC_SUCCESS;
}

int entropy_remote_get(u8_t *buffer, u16_t length)
{
	rp_err_t err;
	uint8_t* packet;
	struct entropy_get_result result = {
		.buffer = buffer,
		.length = length,
	};

	if (!buffer || length < 1) {
		return -EINVAL;
	}

	NRF_RPC_CMD_ALLOC(&entropy_group, &packet, sizeof(uint16_t), return -ENOMEM);

	*(uint16_t *)&packet[0] = length;

	err = NRF_RPC_CMD_SEND(&entropy_group, SER_COMMAND_ENTROPY_GET, packet, sizeof(uint16_t), entropy_get_rsp, &result);
	if (err) {
		return -EINVAL;
	}

	return result.result;
}


static int serialization_init(struct device *dev)
{
	ARG_UNUSED(dev);

	rp_err_t err;

	printk("Init begin\n");

	err = nrf_rpc_init();
	if (err) {
		return -EINVAL;
	}

	printk("Init done\n");

	return 0;
}


SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);

#if 0
struct entropy_rsp {
	u8_t *buffer;
	u16_t length;
	int err_code;
};

RP_SER_DEFINE(entropy_ser, 0, 2048, 3);

static struct entropy_rsp rsp_data;
static void (*async_callback)(u8_t* buffer, size_t length);

static rp_err_t entropy_get_rsp(const uint8_t *packet, size_t packet_len)
{

	if (packet_len != sizeof(int) + rsp_data.length) {
		return RP_ERROR_INTERNAL;
	}
	rsp_data.err_code = *(int *)&packet[0];

	memcpy(rsp_data.buffer, &packet[sizeof(int)], rsp_data.length);

	return RP_SUCCESS;
}

int entropy_remote_get(u8_t *buffer, u16_t length)
{
	rp_err_t err;

	if (!buffer || length < 1) {
		return -EINVAL;
	}

	rsp_data.buffer = buffer;
	rsp_data.length = length;

	RP_SER_CMD_ALLOC(cmd_buf, &entropy_ser, sizeof(uint16_t));

	if (RP_SER_ALLOC_FAILED(cmd_buf)) {
		return -ENOMEM;
	}

	*(uint16_t *)&cmd_buf[0] = length;

	err = rp_ser_cmd_send(&entropy_ser, SER_COMMAND_ENTROPY_GET, cmd_buf, sizeof(uint16_t), entropy_get_rsp);
	if (err) {
		return -EINVAL;
	}

	return rsp_data.err_code;
}

int entropy_remote_get_async(u16_t length, void (*callback)(u8_t* buffer, size_t length))
{
	rp_err_t err;

	if (length < 1) {
		return -EINVAL;
	}

	RP_SER_EVT_ALLOC(evt_buf, &entropy_ser, sizeof(uint16_t));

	if (RP_SER_ALLOC_FAILED(cmd_buf)) {
		return -ENOMEM;
	}

	*(uint16_t *)&evt_buf[0] = length;

	async_callback = callback;

	err = rp_ser_evt_send(&entropy_ser, SER_EVENT_ENTROPY_GET_ASYNC, evt_buf, sizeof(uint16_t));
	if (err) {
		return -EINVAL;
	}

	return rsp_data.err_code;
}


static int serialization_init(struct device *dev)
{
	ARG_UNUSED(dev);

	rp_err_t err;

	err = rp_ser_init(&entropy_ser);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static rp_err_t entropy_get_result_handler(uint8_t code, const uint8_t *packet, size_t packet_len)
{
	int err;
	size_t length;
	u8_t buf[32];

	if (packet_len < sizeof(int)) {
		return RP_ERROR_INTERNAL;
	}
	err = *(int *)&packet[0];
	length = packet_len - sizeof(int);
	if (length > sizeof(buf)) {
		return RP_ERROR_NO_MEM;
	}

	memcpy(buf, &packet[sizeof(int)], length);

	rp_ser_handler_decoding_done(&entropy_ser);

	if (async_callback != NULL) {
		async_callback(buf, length);
	}

	return RP_SUCCESS;
}

RP_SER_EVT_DECODER(entropy_ser, entropy_get_result, SER_EVENT_ENTROPY_GET_ASYNC_RESULT,
		   entropy_get_result_handler);

SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
#endif