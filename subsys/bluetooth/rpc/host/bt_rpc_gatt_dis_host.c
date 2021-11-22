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

struct bt_gatt_discover_container {
	struct bt_gatt_discover_params params;
	uintptr_t remote_pointer;
	union
	{
		struct bt_uuid uuid;
		struct bt_uuid_128 _max_uuid_128;
	};
};

struct bt_gatt_read_container {
	struct bt_gatt_read_params params;
	uintptr_t remote_pointer;
	union
	{
		struct bt_uuid uuid;
		struct bt_uuid_128 _max_uuid_128;
	};
	uint16_t handles[0];
};

struct bt_gatt_write_container {
	struct bt_gatt_write_params params;
	uintptr_t remote_pointer;
	uint8_t data[0];
};

struct bt_gatt_subscribe_container {
	struct bt_gatt_subscribe_params params;
	uintptr_t remote_pointer;
	sys_snode_t node;
};

static sys_slist_t subscribe_containers = SYS_SLIST_STATIC_INIT(&subscribe_containers);

K_MUTEX_DEFINE(subscribe_containers_mutex);

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

static uint8_t bt_gatt_discover_callback(struct bt_conn *conn,
					const struct bt_gatt_attr *attr,
					struct bt_gatt_discover_params *params)
{
	uint8_t result;
	struct bt_gatt_discover_container* container;
	struct nrf_rpc_cbor_ctx ctx;
	
	container = CONTAINER_OF(params, struct bt_gatt_discover_container, params);

	NRF_RPC_CBOR_ALLOC(ctx, 3 + 5 + 18 + 3 + 18 + 3 + 3);

	bt_rpc_encode_bt_conn(&ctx.encoder, conn);
	ser_encode_uint(&ctx.encoder, container->remote_pointer);

	if (attr == NULL) {
		ser_encode_null(&ctx.encoder);
	} else {
		struct bt_uuid_16 *attr_uuid_16 = (struct bt_uuid_16 *)attr->uuid;
		bt_uuid_enc(&ctx.encoder, attr->uuid);
		ser_encode_uint(&ctx.encoder, attr->handle);
		if (attr->user_data == NULL) {
			ser_encode_null(&ctx.encoder);
		} else if (attr->uuid->type != BT_UUID_TYPE_16) {
			goto unsupported_exit;
		} else if (attr_uuid_16->val == BT_UUID_GATT_PRIMARY_VAL ||
			   attr_uuid_16->val == BT_UUID_GATT_SECONDARY_VAL) {
			struct bt_gatt_service_val *service;
			service = (struct bt_gatt_service_val *)attr->user_data;
			bt_uuid_enc(&ctx.encoder, service->uuid);
			ser_encode_uint(&ctx.encoder, service->end_handle);
		} else if (attr_uuid_16->val == BT_UUID_GATT_INCLUDE_VAL) {
			struct bt_gatt_include *include;
			include = (struct bt_gatt_include *)attr->user_data;
			bt_uuid_enc(&ctx.encoder, include->uuid);
			ser_encode_uint(&ctx.encoder, include->start_handle);
			ser_encode_uint(&ctx.encoder, include->end_handle);
		} else if (attr_uuid_16->val == BT_UUID_GATT_CHRC_VAL) {
			struct bt_gatt_chrc *chrc;
			chrc = (struct bt_gatt_chrc *)attr->user_data;
			bt_uuid_enc(&ctx.encoder, chrc->uuid);
			ser_encode_uint(&ctx.encoder, chrc->value_handle);
			ser_encode_uint(&ctx.encoder, chrc->properties);
		} else {
			goto unsupported_exit;
		}
	}

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_DISCOVER_CALLBACK_RPC_CMD,
		&ctx, ser_rsp_decode_u8, &result);

	if (result == BT_GATT_ITER_STOP || attr == NULL) {
		k_free(container);
	}

	return result;

unsupported_exit:
	k_free(container);
	report_encoding_error(BT_GATT_DISCOVER_CALLBACK_RPC_CMD);
	return BT_GATT_ITER_STOP;
}

static void bt_gatt_discover_params_dec(CborValue *value, struct bt_gatt_discover_params *data)
{
	data->uuid = bt_uuid_dec(value, (struct bt_uuid *)data->uuid);
	data->start_handle = ser_decode_uint(value);
	data->end_handle = ser_decode_uint(value);
	data->type = ser_decode_uint(value);
}

static void bt_gatt_discover_rpc_handler(CborValue *value, void *_handler_data)
{
	int result;
	struct bt_conn *conn;
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

uint8_t bt_gatt_read_callback(struct bt_conn *conn, uint8_t err,
			      struct bt_gatt_read_params *params,
			      const void *data, uint16_t length)
{
	uint8_t result;
	struct bt_gatt_read_container* container;
	struct nrf_rpc_cbor_ctx ctx;

	container = CONTAINER_OF(params, struct bt_gatt_read_container, params);

	NRF_RPC_CBOR_ALLOC(ctx, 5 + 3 + 2 + 5 + 5 + length);

	ser_encode_uint(&ctx.encoder, SCRATCHPAD_ALIGN(length));
	bt_rpc_encode_bt_conn(&ctx.encoder, conn);
	ser_encode_uint(&ctx.encoder, err);
	ser_encode_uint(&ctx.encoder, container->remote_pointer);
	ser_encode_buffer(&ctx.encoder, data, length);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_READ_CALLBACK_RPC_CMD,
		&ctx, ser_rsp_decode_u8, &result);

	if (result == BT_GATT_ITER_STOP || data == NULL || params->handle_count == 1) {
		k_free(container);
	}

	return result;
}

static void bt_gatt_read_params_dec(CborValue *value, struct bt_gatt_read_params *data)
{
	if (data->handle_count == 0) {
		data->by_uuid.start_handle = ser_decode_uint(value);
		data->by_uuid.end_handle = ser_decode_uint(value);
		data->by_uuid.uuid = bt_uuid_dec(value, (struct bt_uuid *)data->by_uuid.uuid);
	} else if (data->handle_count == 1) {
		data->single.handle = ser_decode_uint(value);
		data->single.offset = ser_decode_uint(value);
	} else {
		ser_decode_buffer(value, data->multiple.handles,
				  sizeof(data->multiple.handles[0]) * data->handle_count);
		data->multiple.variable = ser_decode_bool(value);
	}
}

static void bt_gatt_read_rpc_handler(CborValue *value, void *_handler_data)
{
	struct bt_conn * conn;
	struct bt_gatt_read_container* container;
	int result;
	size_t handle_count;

	conn = bt_rpc_decode_bt_conn(value);
	handle_count = ser_decode_uint(value);
	container = k_malloc(sizeof(struct bt_gatt_read_container) +
			     sizeof(container->params.multiple.handles[0]) * handle_count);
	if (container == NULL) {
		ser_decoding_done_and_check(value);
		goto alloc_error;
	}
	container->params.handle_count = handle_count;
	container->params.by_uuid.uuid = &container->uuid;
	container->params.multiple.handles = container->handles;

	bt_gatt_read_params_dec(value, &container->params);
	container->remote_pointer = ser_decode_uint(value);
	container->params.func = bt_gatt_read_callback;

	if (!ser_decoding_done_and_check(value)) {
		goto decoding_error;
	}

	result = bt_gatt_read(conn, &container->params);

	if (result < 0) {
		k_free(container);
	}

	ser_rsp_send_int(result);

	return;

decoding_error:
	k_free(container);
alloc_error:
	report_decoding_error(BT_GATT_READ_RPC_CMD, _handler_data);

}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_read, BT_GATT_READ_RPC_CMD,
	bt_gatt_read_rpc_handler, NULL);

void bt_gatt_write_callback(struct bt_conn *conn, uint8_t err,
			    struct bt_gatt_write_params *params)
{
	struct bt_gatt_write_container* container;
	struct nrf_rpc_cbor_ctx ctx;

	container = CONTAINER_OF(params, struct bt_gatt_write_container, params);

	NRF_RPC_CBOR_ALLOC(ctx, 3 + 2 + 5);

	bt_rpc_encode_bt_conn(&ctx.encoder, conn);
	ser_encode_uint(&ctx.encoder, err);
	ser_encode_uint(&ctx.encoder, container->remote_pointer);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_WRITE_CALLBACK_RPC_CMD,
		&ctx, ser_rsp_decode_void, NULL);

	k_free(container);
}

static void bt_gatt_write_rpc_handler(CborValue *value, void *_handler_data)
{

	struct bt_conn * conn;
	struct bt_gatt_write_container* container;
	int result;
	size_t buffer_length;

	conn = bt_rpc_decode_bt_conn(value);
	buffer_length = ser_decode_buffer_size(value);
	container = k_malloc(sizeof(struct bt_gatt_write_container) + buffer_length);
	if (container == NULL) {
		ser_decoding_done_and_check(value);
		goto alloc_error;
	}
	container->params.data = container->data;
	container->params.length = buffer_length;
	ser_decode_buffer(value, (void *)container->params.data, container->params.length);
	container->params.handle = ser_decode_uint(value);
	container->params.offset = ser_decode_uint(value);
	container->remote_pointer = ser_decode_uint(value);
	container->params.func = bt_gatt_write_callback;

	if (!ser_decoding_done_and_check(value)) {
		goto decoding_error;
	}

	result = bt_gatt_write(conn, &container->params);

	if (result < 0) {
		k_free(container);
	}

	ser_rsp_send_int(result);

	return;

decoding_error:
	k_free(container);
alloc_error:
	report_decoding_error(BT_GATT_WRITE_RPC_CMD, _handler_data);

}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_write, BT_GATT_WRITE_RPC_CMD,
	bt_gatt_write_rpc_handler, NULL);

static void bt_gatt_write_without_response_cb_rpc_handler(CborValue *value, void *_handler_data)
{

	struct bt_conn * conn;
	uint16_t handle;
	uint16_t length;
	uint8_t* data;
	bool sign;
	bt_gatt_complete_func_t func;
	void * user_data;
	int result;
	struct ser_scratchpad scratchpad;

	SER_SCRATCHPAD_DECLARE(&scratchpad, value);

	conn = bt_rpc_decode_bt_conn(value);
	handle = ser_decode_uint(value);
	length = ser_decode_uint(value);
	data = ser_decode_buffer_into_scratchpad(&scratchpad);
	sign = ser_decode_bool(value);
	func = (bt_gatt_complete_func_t)ser_decode_callback(value, bt_gatt_complete_func_t_encoder);
	user_data = (void *)ser_decode_uint(value);

	if (!ser_decoding_done_and_check(value)) {
		goto decoding_error;
	}

	result = bt_gatt_write_without_response_cb(conn, handle, data, length, sign, func,
						   user_data);

	ser_rsp_send_int(result);

	return;
decoding_error:
	report_decoding_error(BT_GATT_WRITE_WITHOUT_RESPONSE_CB_RPC_CMD, _handler_data);

}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_write_without_response_cb,
	BT_GATT_WRITE_WITHOUT_RESPONSE_CB_RPC_CMD, bt_gatt_write_without_response_cb_rpc_handler,
	NULL);

static struct bt_gatt_subscribe_container *get_subscribe_container(uintptr_t remote_pointer,
								   bool *create)
{
	struct bt_gatt_subscribe_container *container = NULL;

	k_mutex_lock(&subscribe_containers_mutex, K_FOREVER);

	SYS_SLIST_FOR_EACH_CONTAINER(&subscribe_containers, container, node) {
		if (container->remote_pointer == remote_pointer) {
			*create = false;
			goto unlock_and_return;
		}
	}

	if (*create) {
		container = k_calloc(1, sizeof(struct bt_gatt_subscribe_container));
		if (container != NULL) {
			container->remote_pointer = remote_pointer;
			sys_slist_append(&subscribe_containers, &container->node);
		}
	} else {
		container = NULL;
	}

unlock_and_return:
	k_mutex_unlock(&subscribe_containers_mutex);
	return container;
}


void free_subscribe_container(struct bt_gatt_subscribe_container *container)
{
	k_mutex_lock(&subscribe_containers_mutex, K_FOREVER);
	sys_slist_find_and_remove(&subscribe_containers, &container->node);
	k_mutex_unlock(&subscribe_containers_mutex);
	k_free(container);
}

static uint8_t bt_gatt_subscribe_params_notify(struct bt_conn *conn,
				      struct bt_gatt_subscribe_params *params,
				      const void *data, uint16_t length)
{
	struct nrf_rpc_cbor_ctx ctx;
	size_t _data_size;
	uint8_t result;
	size_t _scratchpad_size = 0;
	size_t _buffer_size_max = 21;
	struct bt_gatt_subscribe_container *container;

	container = CONTAINER_OF(params, struct bt_gatt_subscribe_container, params);

	_data_size = sizeof(uint8_t) * length;
	_buffer_size_max += _data_size;

	_scratchpad_size += SCRATCHPAD_ALIGN(_data_size);

	NRF_RPC_CBOR_ALLOC(ctx, _buffer_size_max);
	ser_encode_uint(&ctx.encoder, _scratchpad_size);

	bt_rpc_encode_bt_conn(&ctx.encoder, conn);
	ser_encode_uint(&ctx.encoder, container->remote_pointer);
	ser_encode_buffer(&ctx.encoder, data, _data_size);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_SUBSCRIBE_PARAMS_NOTIFY_RPC_CMD,
		&ctx, ser_rsp_decode_u8, &result);

	return result;
}

static inline void bt_gatt_subscribe_params_write(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params,
						    uint32_t callback_slot)
{
	struct nrf_rpc_cbor_ctx ctx;
	size_t _params_size;
	size_t _scratchpad_size = 0;
	size_t _buffer_size_max = 26;

	if (params != NULL) {
		_params_size = sizeof(uint8_t) * params->length;
		_buffer_size_max += _params_size;
		_scratchpad_size += SCRATCHPAD_ALIGN(_params_size);
	}

	NRF_RPC_CBOR_ALLOC(ctx, _buffer_size_max);
	ser_encode_uint(&ctx.encoder, _scratchpad_size);

	bt_rpc_encode_bt_conn(&ctx.encoder, conn);
	ser_encode_uint(&ctx.encoder, err);
	if (params != NULL) {
		ser_encode_uint(&ctx.encoder, params->handle);
		ser_encode_uint(&ctx.encoder, params->offset);
		ser_encode_buffer(&ctx.encoder, params->data, params->length);
	} else {
		ser_encode_null(&ctx.encoder);
	}
	ser_encode_uint(&ctx.encoder, callback_slot);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_SUBSCRIBE_PARAMS_WRITE_RPC_CMD,
		&ctx, ser_rsp_decode_void, NULL);
}

CBKPROXY_HANDLER(bt_gatt_subscribe_params_write_encoder, bt_gatt_subscribe_params_write,
		 (struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params), (conn, err, params));

void bt_gatt_subscribe_params_dec(CborValue *value, struct bt_gatt_subscribe_params *data)
{
	data->notify = ser_decode_bool(value) ? bt_gatt_subscribe_params_notify : NULL;
	data->write = (bt_gatt_write_func_t)ser_decode_callback(value, bt_gatt_subscribe_params_write_encoder);
	data->value_handle = ser_decode_uint(value);
	data->ccc_handle = ser_decode_uint(value);
	data->value = ser_decode_uint(value);
	atomic_set(data->flags, (atomic_val_t)ser_decode_uint(value));
}

static void bt_gatt_subscribe_rpc_handler(CborValue *value, void *_handler_data)
{

	struct bt_conn * conn;
	struct bt_gatt_subscribe_container *container;
	bool new_container = true;
	int result;
	uintptr_t remote_pointer;

	conn = bt_rpc_decode_bt_conn(value);
	remote_pointer = ser_decode_uint(value);
	container = get_subscribe_container(remote_pointer, &new_container);
	if (container == NULL) {
		ser_decoding_done_and_check(value);
		goto alloc_error;
	}
	bt_gatt_subscribe_params_dec(value, &container->params);

	if (!ser_decoding_done_and_check(value)) {
		goto decoding_error;
	}

	result = bt_gatt_subscribe(conn, &container->params);

	ser_rsp_send_int(result);

	if (result < 0 && new_container) {
		free_subscribe_container(container);
	}

	return;
decoding_error:
	if (new_container) {
		free_subscribe_container(container);
	}
alloc_error:
	report_decoding_error(BT_GATT_SUBSCRIBE_RPC_CMD, _handler_data);

}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_subscribe, BT_GATT_SUBSCRIBE_RPC_CMD,
	bt_gatt_subscribe_rpc_handler, NULL);

static void bt_gatt_resubscribe_rpc_handler(CborValue *value, void *_handler_data)
{

	uint8_t id;
	bt_addr_le_t _peer_data;
	const bt_addr_le_t * peer;
	int result;
	uintptr_t remote_pointer;
	struct bt_gatt_subscribe_container *container;
	bool new_container = true;

	id = ser_decode_uint(value);
	peer = ser_decode_buffer(value, &_peer_data, sizeof(bt_addr_le_t));
	remote_pointer = ser_decode_uint(value);
	container = get_subscribe_container(remote_pointer, &new_container);
	if (container == NULL) {
		ser_decoding_done_and_check(value);
		goto alloc_error;
	}
	bt_gatt_subscribe_params_dec(value, &container->params);

	if (!ser_decoding_done_and_check(value)) {
		goto decoding_error;
	}

	result = bt_gatt_resubscribe(id, peer, &container->params);

	ser_rsp_send_int(result);

	if (result < 0 && new_container) {
		free_subscribe_container(container);
	}

	return;
decoding_error:
	if (new_container) {
		free_subscribe_container(container);
	}
alloc_error:
	report_decoding_error(BT_GATT_RESUBSCRIBE_RPC_CMD, _handler_data);

}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_resubscribe, BT_GATT_RESUBSCRIBE_RPC_CMD,
	bt_gatt_resubscribe_rpc_handler, NULL);

static void bt_gatt_unsubscribe_rpc_handler(CborValue *value, void *_handler_data)
{

	struct bt_conn * conn;
	int result;
	uintptr_t remote_pointer;
	struct bt_gatt_subscribe_container *container;
	bool new_container = false;
	
	conn = bt_rpc_decode_bt_conn(value);
	remote_pointer = ser_decode_uint(value);

	if (!ser_decoding_done_and_check(value)) {
		goto decoding_error;
	}

	container = get_subscribe_container(remote_pointer, &new_container);

	if (container != NULL) {
		result = bt_gatt_unsubscribe(conn, &container->params);
		free_subscribe_container(container);
	} else {
		result = -EINVAL;
	}

	ser_rsp_send_int(result);

	return;
decoding_error:
	report_decoding_error(BT_GATT_UNSUBSCRIBE_RPC_CMD, _handler_data);

}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_unsubscribe, BT_GATT_UNSUBSCRIBE_RPC_CMD,
	bt_gatt_unsubscribe_rpc_handler, NULL);

static void bt_rpc_gatt_subscribe_flag_update_rpc_handler(CborValue *value, void *_handler_data)
{
	uint32_t flags_bit;
	int result;
	uintptr_t remote_pointer;
	struct bt_gatt_subscribe_container *container;
	bool new_container = false;
	int val;

	remote_pointer = ser_decode_uint(value);
	flags_bit = ser_decode_uint(value);
	val = ser_decode_int(value);

	if (!ser_decoding_done_and_check(value)) {
		goto decoding_error;
	}

	container = get_subscribe_container(remote_pointer, &new_container);
	if (container == NULL) {
		result = -EINVAL;
	} else {
		if (atomic_test_bit(container->params.flags, flags_bit)) {
			result = 1;
		} else {
			result = 0;
		}

		if (val == 0) {
			atomic_clear_bit(container->params.flags, flags_bit);
		} else if (val > 0) {
			atomic_set_bit(container->params.flags, flags_bit);
		}
	}

	ser_rsp_send_int(result);

	return;
decoding_error:
	report_decoding_error(BT_RPC_GATT_SUBSCRIBE_FLAG_UPDATE_RPC_CMD, _handler_data);

}

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_rpc_gatt_subscribe_flag_update, BT_RPC_GATT_SUBSCRIBE_FLAG_UPDATE_RPC_CMD,
	bt_rpc_gatt_subscribe_flag_update_rpc_handler, NULL);
