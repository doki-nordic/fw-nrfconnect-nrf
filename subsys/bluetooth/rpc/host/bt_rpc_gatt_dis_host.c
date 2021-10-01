/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include "zephyr.h"

#include "nrf_rpc_cbor.h"

#include "bluetooth/bluetooth.h"
#include "bluetooth/att.h"
#include "bluetooth/gatt.h"
#include "bluetooth/conn.h"

#include "bt_rpc_common.h"
#include "bt_rpc_gatt_common.h"
#include "serialize.h"
#include "cbkproxy.h"

#include <logging/log.h>


LOG_MODULE_DECLARE(BT_RPC, CONFIG_BT_RPC_LOG_LEVEL);

static void report_decoding_error(uint8_t cmd_evt_id, void *data)
{
	nrf_rpc_err(-EBADMSG, NRF_RPC_ERR_SRC_RECV, &bt_rpc_grp, cmd_evt_id,
		    NRF_RPC_PACKET_TYPE_CMD);
}

static void report_encoding_error(uint8_t cmd_evt_id)
{
	nrf_rpc_err(-EBADMSG, NRF_RPC_ERR_SRC_RECV, &bt_rpc_grp, cmd_evt_id,
		    NRF_RPC_PACKET_TYPE_CMD);
}

static struct bt_uuid *bt_uuid_dec(CborValue *value, struct bt_uuid *uuid)
{
	return (struct bt_uuid *)ser_decode_buffer(value, uuid, sizeof(struct bt_uuid_128));
}

static void bt_uuid_enc(CborEncoder *encoder, const struct bt_uuid *uuid)
{
	size_t size = 0;
	if (uuid != NULL) {
		if (uuid->type == BT_UUID_TYPE_16) {
			size = sizeof(struct bt_uuid_128);
		} else if (uuid->type == BT_UUID_TYPE_32) {
			size = sizeof(struct bt_uuid_32);
		} else if (uuid->type == BT_UUID_TYPE_128) {
			size = sizeof(struct bt_uuid_128);
		} else {
			ser_encoder_invalid(encoder);
			return;
		}
	}
	ser_encode_buffer(encoder, uuid, size);
}

/*--------------- bt_gatt_discover ---------------*/

struct bt_gatt_discover_container {
	struct bt_gatt_discover_params params;
	uintptr_t remote_pointer;
	union
	{
		struct bt_uuid uuid;
		struct bt_uuid_128 _max_uuid_128;
	};
};

static uint8_t bt_gatt_discover_callback(struct bt_conn *conn,
					const struct bt_gatt_attr *attr,
					struct bt_gatt_discover_params *params)
{
	uint8_t result;
	struct bt_gatt_discover_container* container;
	struct nrf_rpc_cbor_ctx _ctx;
	
	container = CONTAINER_OF(params, struct bt_gatt_discover_container, params);

	NRF_RPC_CBOR_ALLOC(_ctx, 3 + 5 + 18 + 3 + 18 + 3 + 3);

	bt_rpc_encode_bt_conn(&_ctx.encoder, conn);
	ser_encode_uint(&_ctx.encoder, container->remote_pointer);

	if (attr == NULL) {
		ser_encode_null(&_ctx.encoder);
	} else {
		struct bt_uuid_16 *attr_uuid_16 = (struct bt_uuid_16 *)attr->uuid;
		bt_uuid_enc(&_ctx.encoder, attr->uuid);
		ser_encode_uint(&_ctx.encoder, attr->handle);
		if (attr->user_data == NULL) {
			ser_encode_null(&_ctx.encoder);
		} else if (attr->uuid->type != BT_UUID_TYPE_16) {
			goto unsupported_exit;
		} else if (attr_uuid_16->val == BT_UUID_GATT_PRIMARY_VAL || attr_uuid_16->val == BT_UUID_GATT_SECONDARY_VAL) {
			struct bt_gatt_service_val *service = (struct bt_gatt_service_val *)attr->user_data;
			bt_uuid_enc(&_ctx.encoder, service->uuid);
			ser_encode_uint(&_ctx.encoder, service->end_handle);
		} else if (attr_uuid_16->val == BT_UUID_GATT_INCLUDE_VAL) {
			struct bt_gatt_include *include = (struct bt_gatt_include *)attr->user_data;
			bt_uuid_enc(&_ctx.encoder, include->uuid);
			ser_encode_uint(&_ctx.encoder, include->start_handle);
			ser_encode_uint(&_ctx.encoder, include->end_handle);
		} else if (attr_uuid_16->val == BT_UUID_GATT_CHRC_VAL) {
			struct bt_gatt_chrc *chrc = (struct bt_gatt_chrc *)attr->user_data;
			bt_uuid_enc(&_ctx.encoder, chrc->uuid);
			ser_encode_uint(&_ctx.encoder, chrc->value_handle);
			ser_encode_uint(&_ctx.encoder, chrc->properties);
		} else {
			goto unsupported_exit;
		}
	}

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_DISCOVER_CALLBACK_RPC_CMD,
		&_ctx, ser_rsp_decode_u8, &result);

	if (result == BT_GATT_ITER_STOP || attr == NULL) {
		k_free(container);
	}

	return result;

unsupported_exit:
	k_free(container);
	report_encoding_error(BT_GATT_DISCOVER_CALLBACK_RPC_CMD);
	return BT_GATT_ITER_STOP;
}

static void bt_gatt_discover_params_dec(CborValue *_value, struct bt_gatt_discover_params *_data)
{
	_data->uuid = bt_uuid_dec(_value, (struct bt_uuid *)_data->uuid);
	_data->func = (bt_gatt_discover_func_t)ser_decode_uint(_value);
	_data->start_handle = ser_decode_uint(_value);
	_data->end_handle = ser_decode_uint(_value);
	_data->type = ser_decode_uint(_value);
}

static void bt_gatt_discover_rpc_handler(CborValue *value, void *_handler_data)
{
	int result;
	struct bt_conn * conn;
	struct bt_gatt_discover_container* container;

	container = k_malloc(sizeof(struct bt_gatt_discover_container));
	if (container == NULL) {
		ser_decoding_done_and_check(value);
		goto alloc_error;
	}
	container->params.uuid = &container->uuid;

	conn = bt_rpc_decode_bt_conn(value);
	bt_gatt_discover_params_dec(value, &container->params);
	container->remote_pointer = ser_decode_uint(value);

	container->params.func = bt_gatt_discover_callback;

	if (!ser_decoding_done_and_check(value)) {
		goto decoding_error;
	}

	result = bt_gatt_discover(conn, &container->params);

	if (result < 0) {
		k_free(container);
	}

	ser_rsp_send_int(result);

	return;

decoding_error:
	k_free(container);
alloc_error:
	report_decoding_error(BT_GATT_DISCOVER_RPC_CMD, _handler_data);
}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_discover, BT_GATT_DISCOVER_RPC_CMD,
	bt_gatt_discover_rpc_handler, NULL);

