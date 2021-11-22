/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include "zephyr.h"

#include "nrf_rpc_cbor.h"

#include "bluetooth/bluetooth.h"
#include "bluetooth/att.h"
#include "bluetooth/gatt.h"

#include "bt_rpc_common.h"
#include "bt_rpc_gatt_common.h"
#include "serialize.h"
#include "cbkproxy.h"

#include <logging/log.h>

LOG_MODULE_DECLARE(BT_RPC, CONFIG_BT_RPC_LOG_LEVEL);

#if defined(CONFIG_BT_GATT_AUTO_DISCOVER_CCC)
#error "CONFIG_BT_GATT_AUTO_DISCOVER_CCC is not supported by the RPC GATT"
#endif

static void report_decoding_error(uint8_t cmd_evt_id, void *data)
{
	nrf_rpc_err(-EBADMSG, NRF_RPC_ERR_SRC_RECV, &bt_rpc_grp, cmd_evt_id,
		    NRF_RPC_PACKET_TYPE_CMD);
}

static struct bt_uuid *bt_uuid_dec(CborValue *value, struct bt_uuid *uuid)
{
	return (struct bt_uuid *)ser_decode_buffer(value, uuid, sizeof(struct bt_uuid_128));
}

static size_t bt_uuid_enc(CborEncoder *encoder, const struct bt_uuid *uuid)
{
	size_t size = 0;
	if (uuid != NULL) {
		if (uuid->type == BT_UUID_TYPE_16) {
			size = sizeof(struct bt_uuid_16);
		} else if (uuid->type == BT_UUID_TYPE_32) {
			size = sizeof(struct bt_uuid_32);
		} else if (uuid->type == BT_UUID_TYPE_128) {
			size = sizeof(struct bt_uuid_128);
		} else {
			if (encoder != NULL) {
				ser_encoder_invalid(encoder);
			}
			return 1;
		}
	}
	if (encoder != NULL) {
		ser_encode_buffer(encoder, uuid, size);
	}
	return 1 + size;
}

static size_t bt_gatt_discover_params_buf_size(const struct bt_gatt_discover_params *data)
{
	return bt_uuid_enc(NULL, data->uuid) + 3 + 3 + 3;
}

static void bt_gatt_discover_params_enc(CborEncoder *encoder,
	const struct bt_gatt_discover_params *data)
{
	bt_uuid_enc(encoder, data->uuid);
	ser_encode_uint(encoder, data->start_handle);
	ser_encode_uint(encoder, data->end_handle);
	ser_encode_uint(encoder, data->type);
}

int bt_gatt_discover(struct bt_conn *conn, struct bt_gatt_discover_params *params)
{
	struct nrf_rpc_cbor_ctx ctx;
	int result;

	NRF_RPC_CBOR_ALLOC(ctx, 3 + bt_gatt_discover_params_buf_size(params) + 5);

	bt_rpc_encode_bt_conn(&ctx.encoder, conn);
	bt_gatt_discover_params_enc(&ctx.encoder, params);
	ser_encode_uint(&ctx.encoder, (uintptr_t)params);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_DISCOVER_RPC_CMD,
		&ctx, ser_rsp_decode_i32, &result);

	return result;
}

static void bt_gatt_discover_callback_rpc_handler(CborValue *value, void *_handler_data)
{

	struct bt_conn *conn;
	uintptr_t params_pointer;
	uint8_t result;
	struct bt_gatt_discover_params *params;
	struct bt_uuid_16 *attr_uuid_16;
	union
	{
		struct bt_uuid uuid;
		struct bt_uuid_128 _uuid_max;
	} uuid_buffers[2];
	union
	{
		struct bt_gatt_service_val service;
		struct bt_gatt_include include;
		struct bt_gatt_chrc chrc;
	} user_data;
	struct bt_gatt_attr attr_instance = {
		.user_data = &user_data,
	};
	struct bt_gatt_attr *attr = &attr_instance;

	conn = bt_rpc_decode_bt_conn(value);
	params_pointer = ser_decode_uint(value);
	params = (struct bt_gatt_discover_params *)params_pointer;

	if (ser_decode_is_null(value)) {
		ser_decode_skip(value);
		attr = NULL;
	} else {
		attr->uuid = bt_uuid_dec(value, &uuid_buffers[0].uuid);
		attr->handle = ser_decode_uint(value);
		attr_uuid_16 = (struct bt_uuid_16 *)attr->uuid;
		if (ser_decode_is_null(value)) {
			ser_decode_skip(value);
			attr->user_data = NULL;
		} else if (attr->uuid == NULL || attr->uuid->type != BT_UUID_TYPE_16) {
			LOG_ERR("Invalid attribute UUID");
			goto decoding_done_with_error;
		} else if (attr_uuid_16->val == BT_UUID_GATT_PRIMARY_VAL ||
			attr_uuid_16->val == BT_UUID_GATT_SECONDARY_VAL) {
			user_data.service.uuid = bt_uuid_dec(value, &uuid_buffers[1].uuid);
			user_data.service.end_handle = ser_decode_uint(value);
		} else if (attr_uuid_16->val == BT_UUID_GATT_INCLUDE_VAL) {
			user_data.include.uuid = bt_uuid_dec(value, &uuid_buffers[1].uuid);
			user_data.include.start_handle = ser_decode_uint(value);
			user_data.include.end_handle = ser_decode_uint(value);
		} else if (attr_uuid_16->val == BT_UUID_GATT_CHRC_VAL) {
			user_data.chrc.uuid = bt_uuid_dec(value, &uuid_buffers[1].uuid);
			user_data.chrc.value_handle = ser_decode_uint(value);
			user_data.chrc.properties = ser_decode_uint(value);
		} else {
			LOG_ERR("Unsupported attribute UUID");
			goto decoding_done_with_error;
		}
	}

	if (!ser_decoding_done_and_check(value)) {
		goto decoding_error;
	}

	result = params->func(conn, attr, params);

	ser_rsp_send_uint(result);

	return;

decoding_done_with_error:
	ser_decoding_done_and_check(value);
decoding_error:
	report_decoding_error(BT_GATT_DISCOVER_CALLBACK_RPC_CMD, _handler_data);
}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_discover_callback, BT_GATT_DISCOVER_CALLBACK_RPC_CMD,
	bt_gatt_discover_callback_rpc_handler, NULL);

static size_t bt_gatt_read_params_buf_size(const struct bt_gatt_read_params *data)
{
	size_t size = 5 + 5;
	if (data->handle_count == 0) {
		size += 3 + 3 + bt_uuid_enc(NULL, data->by_uuid.uuid);
	} else if (data->handle_count == 1) {
		size += 3 + 3;
	} else {
		size += 5 + sizeof(data->multiple.handles[0]) * data->handle_count + 1;
	}
	return size;
}

static void bt_gatt_read_params_enc(CborEncoder *encoder,
	const struct bt_gatt_read_params *data)
{
	ser_encode_uint(encoder, data->handle_count);
	if (data->handle_count == 0) {
		ser_encode_uint(encoder, data->by_uuid.start_handle);
		ser_encode_uint(encoder, data->by_uuid.end_handle);
		bt_uuid_enc(encoder, data->by_uuid.uuid);
	} else if (data->handle_count == 1) {
		ser_encode_uint(encoder, data->single.handle);
		ser_encode_uint(encoder, data->single.offset);
	} else {
		ser_encode_buffer(encoder, data->multiple.handles,
				  sizeof(data->multiple.handles[0]) * data->handle_count);
		ser_encode_bool(encoder, data->multiple.variable);
	}
}

int bt_gatt_read(struct bt_conn *conn, struct bt_gatt_read_params *params)
{
	struct nrf_rpc_cbor_ctx ctx;
	int result;

	NRF_RPC_CBOR_ALLOC(ctx, 3 + bt_gatt_read_params_buf_size(params) + 5);

	bt_rpc_encode_bt_conn(&ctx.encoder, conn);
	bt_gatt_read_params_enc(&ctx.encoder, params);
	ser_encode_uint(&ctx.encoder, (uintptr_t)params);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_READ_RPC_CMD,
		&ctx, ser_rsp_decode_i32, &result);

	return result;
}

static void bt_gatt_read_callback_rpc_handler(CborValue *value, void *_handler_data)
{
	struct ser_scratchpad scratchpad;
	struct bt_conn *conn;
	uintptr_t params_pointer;
	uint8_t err;
	uint8_t result;
	struct bt_gatt_read_params *params;
	void *data;
	uint16_t length;

	SER_SCRATCHPAD_DECLARE(&scratchpad, value);

	conn = bt_rpc_decode_bt_conn(value);
	err = ser_decode_uint(value);
	params_pointer = ser_decode_uint(value);
	params = (struct bt_gatt_read_params *)params_pointer;

	length = ser_decode_buffer_size(value);
	data = ser_decode_buffer_into_scratchpad(&scratchpad);

	if (!ser_decoding_done_and_check(value)) {
		goto decoding_error;
	}

	result = params->func(conn, err, params, data, length);

	ser_rsp_send_uint(result);

	return;

decoding_error:
	report_decoding_error(BT_GATT_READ_CALLBACK_RPC_CMD, _handler_data);

}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_read_callback, BT_GATT_READ_CALLBACK_RPC_CMD,
	bt_gatt_read_callback_rpc_handler, NULL);

static size_t bt_gatt_write_params_buf_size(const struct bt_gatt_write_params *data)
{
	return 5 + data->length + 3 + 3;
}

static void bt_gatt_write_params_enc(CborEncoder *encoder,
	const struct bt_gatt_write_params *data)
{
	ser_encode_buffer(encoder, data->data, data->length);
	ser_encode_uint(encoder, data->handle);
	ser_encode_uint(encoder, data->offset);
}

int bt_gatt_write(struct bt_conn *conn, struct bt_gatt_write_params *params)
{
	struct nrf_rpc_cbor_ctx ctx;
	int result;

	NRF_RPC_CBOR_ALLOC(ctx, 3 + bt_gatt_write_params_buf_size(params) + 5);

	bt_rpc_encode_bt_conn(&ctx.encoder, conn);
	bt_gatt_write_params_enc(&ctx.encoder, params);
	ser_encode_uint(&ctx.encoder, (uintptr_t)params);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_WRITE_RPC_CMD,
		&ctx, ser_rsp_decode_i32, &result);

	return result;
}

static void bt_gatt_write_callback_rpc_handler(CborValue *value, void *_handler_data)
{
	struct bt_conn *conn;
	uint8_t err;
	struct bt_gatt_write_params *params;

	conn = bt_rpc_decode_bt_conn(value);
	err = ser_decode_uint(value);
	params = (struct bt_gatt_write_params *)ser_decode_uint(value);

	if (!ser_decoding_done_and_check(value)) {
		goto decoding_error;
	}

	params->func(conn, err, params);

	ser_rsp_send_void();

	return;

decoding_error:
	report_decoding_error(BT_GATT_WRITE_CALLBACK_RPC_CMD, _handler_data);

}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_write_callback, BT_GATT_WRITE_CALLBACK_RPC_CMD,
	bt_gatt_write_callback_rpc_handler, NULL);

int bt_gatt_write_without_response_cb(struct bt_conn *conn, uint16_t handle,
				      const void *data, uint16_t length,
				      bool sign, bt_gatt_complete_func_t func,
				      void *user_data)
{
	struct nrf_rpc_cbor_ctx ctx;
	size_t _data_size;
	int result;
	size_t _scratchpad_size = 0;
	size_t _buffer_size_max = 30;

	_data_size = sizeof(uint8_t) * length;
	_buffer_size_max += _data_size;

	_scratchpad_size += SCRATCHPAD_ALIGN(_data_size);

	NRF_RPC_CBOR_ALLOC(ctx, _buffer_size_max);
	ser_encode_uint(&ctx.encoder, _scratchpad_size);

	bt_rpc_encode_bt_conn(&ctx.encoder, conn);
	ser_encode_uint(&ctx.encoder, handle);
	ser_encode_uint(&ctx.encoder, length);
	ser_encode_buffer(&ctx.encoder, data, _data_size);
	ser_encode_bool(&ctx.encoder, sign);
	ser_encode_callback(&ctx.encoder, func);
	ser_encode_uint(&ctx.encoder, (uintptr_t)user_data);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_WRITE_WITHOUT_RESPONSE_CB_RPC_CMD,
		&ctx, ser_rsp_decode_i32, &result);

	return result;
}

void bt_gatt_subscribe_params_enc(CborEncoder *encoder, const struct bt_gatt_subscribe_params *data)
{
	ser_encode_bool(encoder, data->notify != NULL);
	ser_encode_callback(encoder, data->write);
	ser_encode_uint(encoder, data->value_handle);
	ser_encode_uint(encoder, data->ccc_handle);
	ser_encode_uint(encoder, data->value);
	ser_encode_uint(encoder, (uint16_t)atomic_get(data->flags));
}

int bt_gatt_subscribe(struct bt_conn *conn,
		      struct bt_gatt_subscribe_params *params)
{
	struct nrf_rpc_cbor_ctx ctx;
	int result;

	NRF_RPC_CBOR_ALLOC(ctx, 26);

	bt_rpc_encode_bt_conn(&ctx.encoder, conn);
	ser_encode_uint(&ctx.encoder, (uintptr_t)params);
	bt_gatt_subscribe_params_enc(&ctx.encoder, params);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_SUBSCRIBE_RPC_CMD,
		&ctx, ser_rsp_decode_i32, &result);

	return result;
}

int bt_gatt_resubscribe(uint8_t id, const bt_addr_le_t *peer,
			struct bt_gatt_subscribe_params *params)
{

	struct nrf_rpc_cbor_ctx ctx;
	int result;
	size_t _buffer_size_max = 5;

	_buffer_size_max += peer ? sizeof(bt_addr_le_t) : 0;

	_buffer_size_max += 12;

	NRF_RPC_CBOR_ALLOC(ctx, _buffer_size_max);

	ser_encode_uint(&ctx.encoder, id);
	ser_encode_buffer(&ctx.encoder, peer, sizeof(bt_addr_le_t));

	ser_encode_uint(&ctx.encoder, (uintptr_t)params);
	bt_gatt_subscribe_params_enc(&ctx.encoder, params);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_RESUBSCRIBE_RPC_CMD,
		&ctx, ser_rsp_decode_i32, &result);

	return result;
}

int bt_gatt_unsubscribe(struct bt_conn *conn,
			struct bt_gatt_subscribe_params *params)
{
	struct nrf_rpc_cbor_ctx ctx;
	int result;
	size_t _buffer_size_max = 8;

	NRF_RPC_CBOR_ALLOC(ctx, _buffer_size_max);

	bt_rpc_encode_bt_conn(&ctx.encoder, conn);
	ser_encode_uint(&ctx.encoder, (uintptr_t)params);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_UNSUBSCRIBE_RPC_CMD,
		&ctx, ser_rsp_decode_i32, &result);

	return result;
}

static int bt_rpc_gatt_subscribe_flag_update(struct bt_gatt_subscribe_params *params,
					     uint32_t flags_bit, int val)
{
	struct nrf_rpc_cbor_ctx ctx;
	int result;
	size_t _buffer_size_max = 15;

	NRF_RPC_CBOR_ALLOC(ctx, _buffer_size_max);

	ser_encode_uint(&ctx.encoder, (uintptr_t)params);
	ser_encode_uint(&ctx.encoder, flags_bit);
	ser_encode_int(&ctx.encoder, val);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_RPC_GATT_SUBSCRIBE_FLAG_UPDATE_RPC_CMD,
		&ctx, ser_rsp_decode_i32, &result);

	return result;
}

int bt_rpc_gatt_subscribe_flag_set(struct bt_gatt_subscribe_params *params, uint32_t flags_bit)
{
	return bt_rpc_gatt_subscribe_flag_update(params, flags_bit, 1);
}

int bt_rpc_gatt_subscribe_flag_clear(struct bt_gatt_subscribe_params *params, uint32_t flags_bit)
{
	return bt_rpc_gatt_subscribe_flag_update(params, flags_bit, 0);
}

int bt_rpc_gatt_subscribe_flag_get(struct bt_gatt_subscribe_params *params, uint32_t flags_bit)
{
	return bt_rpc_gatt_subscribe_flag_update(params, flags_bit, -1);
}

static void bt_gatt_subscribe_params_notify_rpc_handler(CborValue *value, void *_handler_data)
{

	struct bt_conn * conn;
	struct bt_gatt_subscribe_params * params;
	uint16_t length;
	uint8_t* data;
	uint8_t result = BT_GATT_ITER_CONTINUE;
	struct ser_scratchpad scratchpad;

	SER_SCRATCHPAD_DECLARE(&scratchpad, value);

	conn = bt_rpc_decode_bt_conn(value);
	params = (struct bt_gatt_subscribe_params *)ser_decode_uint(value);
	length = ser_decode_buffer_size(value);
	data = ser_decode_buffer_into_scratchpad(&scratchpad);

	if (!ser_decoding_done_and_check(value)) {
		goto decoding_error;
	}

	if (params->notify != NULL) {
		result = params->notify(conn, params, data, length);
	}

	ser_rsp_send_uint(result);

	return;
decoding_error:
	report_decoding_error(BT_GATT_SUBSCRIBE_PARAMS_NOTIFY_RPC_CMD, _handler_data);

}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_subscribe_params_notify,
	BT_GATT_SUBSCRIBE_PARAMS_NOTIFY_RPC_CMD, bt_gatt_subscribe_params_notify_rpc_handler, NULL);

static void bt_gatt_subscribe_params_write_rpc_handler(CborValue *value, void *_handler_data)
{

	struct bt_conn * conn;
	uint8_t err;
	struct bt_gatt_write_params params;
	struct bt_gatt_write_params *params_ptr;
	struct ser_scratchpad scratchpad;
	bt_gatt_write_func_t func;

	SER_SCRATCHPAD_DECLARE(&scratchpad, value);

	conn = bt_rpc_decode_bt_conn(value);
	err = ser_decode_uint(value);
	if (ser_decode_is_null(value)) {
		ser_decode_skip(value);
		params_ptr = NULL;
	} else {
		params.handle = ser_decode_uint(value);
		params.offset = ser_decode_uint(value);
		params.length = ser_decode_buffer_size(value);
		params.data = ser_decode_buffer_into_scratchpad(&scratchpad);
		params_ptr = &params;
	}
	func = (bt_gatt_write_func_t)ser_decode_callback_call(value);

	if (!ser_decoding_done_and_check(value)) {
		goto decoding_error;
	}

	if (func != NULL) {
		func(conn, err, params_ptr);
	}
	ser_rsp_send_void();

	return;
decoding_error:
	report_decoding_error(BT_GATT_SUBSCRIBE_PARAMS_WRITE_RPC_CMD, _handler_data);

}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_subscribe_params_write, BT_GATT_SUBSCRIBE_PARAMS_WRITE_RPC_CMD,
	bt_gatt_subscribe_params_write_rpc_handler, NULL);
