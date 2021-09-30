/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/* Client side of bluetooth API over nRF RPC.
 */

#include <sys/types.h>

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

SERIALIZE(GROUP(bt_rpc_grp));
SERIALIZE(OPAQUE_STRUCT(void));
SERIALIZE(OPAQUE_STRUCT(struct bt_gatt_exchange_params));
SERIALIZE(FILTERED_STRUCT(struct bt_conn, 3, bt_rpc_encode_bt_conn, bt_rpc_decode_bt_conn));


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

/*--------------- bt_gatt_discover ---------------*/

static size_t bt_gatt_discover_params_buf_size(const struct bt_gatt_discover_params *_data)
{
	return bt_uuid_enc(NULL, _data->uuid) + 5 + 3 * 3;
}

static void bt_gatt_discover_params_enc(CborEncoder *_encoder, const struct bt_gatt_discover_params *_data)
{
	bt_uuid_enc(_encoder, _data->uuid);
	ser_encode_uint(_encoder, (uintptr_t)_data->func);
	ser_encode_uint(_encoder, _data->start_handle);
	ser_encode_uint(_encoder, _data->end_handle);
	ser_encode_uint(_encoder, _data->type);
}


int bt_gatt_discover(struct bt_conn *conn, struct bt_gatt_discover_params *params)
{
	struct nrf_rpc_cbor_ctx _ctx;
	int _result;

	NRF_RPC_CBOR_ALLOC(_ctx, 3 + bt_gatt_discover_params_buf_size(params));

	bt_rpc_encode_bt_conn(&_ctx.encoder, conn);
	bt_gatt_discover_params_enc(&_ctx.encoder, params);
	ser_encode_uint(&_ctx.encoder, (uintptr_t)params);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_DISCOVER_RPC_CMD,
		&_ctx, ser_rsp_decode_i32, &_result);

	return _result;
}

static void bt_gatt_discover_callback_rpc_handler(CborValue *_value, void *_handler_data)
{

	struct bt_conn *conn;
	uintptr_t params_pointer;
	uint8_t result;
	struct bt_gatt_discover_params *params;
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

	conn = bt_rpc_decode_bt_conn(_value);
	params_pointer = ser_decode_uint(_value);
	params = (struct bt_gatt_discover_params *)params_pointer;

	if (ser_decode_is_null(_value)) {
		ser_decode_skip(_value);
		attr = NULL;
	} else {
		struct bt_uuid_16 *attr_uuid_16 = (struct bt_uuid_16 *)attr->uuid;
		attr->uuid = bt_uuid_dec(_value, &uuid_buffers[0].uuid);
		attr->handle = ser_decode_uint(_value);
		if (ser_decode_is_null(_value)) {
			ser_decode_skip(_value);
			attr->user_data = NULL;
		} else if (attr->uuid->type != BT_UUID_TYPE_16) {
			goto decoding_done_with_error;
		} else if (attr_uuid_16->val == BT_UUID_GATT_PRIMARY_VAL || attr_uuid_16->val == BT_UUID_GATT_SECONDARY_VAL) {
			user_data.service.uuid = bt_uuid_dec(_value, &uuid_buffers[1].uuid);
			user_data.service.end_handle = ser_decode_uint(_value);
		} else if (attr_uuid_16->val == BT_UUID_GATT_INCLUDE_VAL) {
			user_data.include.uuid = bt_uuid_dec(_value, &uuid_buffers[1].uuid);
			user_data.include.start_handle = ser_decode_uint(_value);
			user_data.include.end_handle = ser_decode_uint(_value);
		} else if (attr_uuid_16->val == BT_UUID_GATT_CHRC_VAL) {
			user_data.chrc.uuid = bt_uuid_dec(_value, &uuid_buffers[1].uuid);
			user_data.chrc.value_handle = ser_decode_uint(_value);
			user_data.chrc.properties = ser_decode_uint(_value);
		} else {
			goto decoding_done_with_error;
		}
	}

	if (!ser_decoding_done_and_check(_value)) {
		goto decoding_error;
	}

	result = params->func(conn, attr, params);

	ser_rsp_send_uint(result);

	return;
decoding_done_with_error:
	ser_decoding_done_and_check(_value);
decoding_error:
	report_decoding_error(BT_GATT_DISCOVER_CALLBACK_RPC_CMD, _handler_data);

}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_discover_callback, BT_GATT_DISCOVER_CALLBACK_RPC_CMD,
	bt_gatt_discover_callback_rpc_handler, NULL);

