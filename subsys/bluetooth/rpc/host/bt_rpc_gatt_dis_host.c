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

#if defined(CONFIG_BT_GATT_AUTO_DISCOVER_CCC)
#error "CONFIG_BT_GATT_AUTO_DISCOVER_CCC is not supported by the RPC GATT"
#endif

#if CONFIG_BT_RPC_LOG_LEVEL >= 4

static size_t total_allocated = 0;

static void *tracked_alloc(size_t size) {
	size_t *ptr = k_malloc(size + sizeof(size_t));
	if (ptr == NULL) {
		LOG_ERR("Out of memory, allocating %d", size);
		printk("Out of memory, allocating %d\n", size);
		return ptr;
	}
	total_allocated += size;
	*ptr = size;
	LOG_DBG("ALLOC @0x%08X, size %d, total %d", (unsigned int)ptr, size, total_allocated);
	printk("ALLOC @0x%08X, size %d, total %d\n", (unsigned int)ptr, size, total_allocated);
	return &ptr[1];
}

static void *tracked_calloc(size_t size, size_t num) {
	size *= num;
	size_t *ptr = k_calloc(1, size + sizeof(size_t));
	if (ptr == NULL) {
		LOG_ERR("Out of memory, allocating %d", size);
		printk("Out of memory, allocating %d\n", size);
		return ptr;
	}
	total_allocated += size;
	*ptr = size;
	LOG_DBG("CALLOC @0x%08X, size %d, total %d", (unsigned int)ptr, size, total_allocated);
	printk("CALLOC @0x%08X, size %d, total %d\n", (unsigned int)ptr, size, total_allocated);
	return &ptr[1];
}

static void tracked_free(void* ptr) {
	size_t *ptr_size;
	if (ptr == NULL) {
		LOG_DBG("FREE NULL");
		return;
	}
	ptr_size = ptr;
	ptr_size--;
	total_allocated -= *ptr_size;
	LOG_DBG("FREE   @0x%08X, size %d, total %d", (unsigned int)ptr_size, *ptr_size, total_allocated);
	printk("FREE   @0x%08X, size %d, total %d\n", (unsigned int)ptr_size, *ptr_size, total_allocated);
	k_free(ptr_size);
}

#define k_calloc tracked_calloc
#define k_malloc tracked_alloc
#define k_free tracked_free

#endif


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



/*--------------- bt_gatt_read ---------------*/

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

uint8_t bt_gatt_read_callback(struct bt_conn *conn, uint8_t err,
			      struct bt_gatt_read_params *params,
			      const void *data, uint16_t length)
{
	uint8_t result;
	struct bt_gatt_read_container* container;
	struct nrf_rpc_cbor_ctx _ctx;

	container = CONTAINER_OF(params, struct bt_gatt_read_container, params);

	NRF_RPC_CBOR_ALLOC(_ctx, 5 + 3 + 2 + 5 + 5 + length);

	ser_encode_uint(&_ctx.encoder, SCRATCHPAD_ALIGN(length));
	bt_rpc_encode_bt_conn(&_ctx.encoder, conn);
	ser_encode_uint(&_ctx.encoder, err);
	ser_encode_uint(&_ctx.encoder, container->remote_pointer);
	ser_encode_buffer(&_ctx.encoder, data, length);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_READ_CALLBACK_RPC_CMD,
		&_ctx, ser_rsp_decode_u8, &result);

	if (result == BT_GATT_ITER_STOP || data == NULL || params->handle_count == 1) {
		k_free(container);
	}

	return result;
}

static void bt_gatt_read_params_dec(CborValue *_value, struct bt_gatt_read_params *_data)
{
	if (_data->handle_count == 0) {
		_data->by_uuid.start_handle = ser_decode_uint(_value);
		_data->by_uuid.end_handle = ser_decode_uint(_value);
		_data->by_uuid.uuid = bt_uuid_dec(_value, (struct bt_uuid *)_data->by_uuid.uuid);
	} else if (_data->handle_count == 1) {
		_data->single.handle = ser_decode_uint(_value);
		_data->single.offset = ser_decode_uint(_value);
	} else {
		ser_decode_buffer(_value, _data->multiple.handles,
				  sizeof(_data->multiple.handles[0]) * _data->handle_count);
		_data->multiple.variable = ser_decode_bool(_value);
	}
}

static void bt_gatt_read_rpc_handler(CborValue *_value, void *_handler_data)     /*####%BpLH*/
{                                                                                /*#####@IVE*/

	struct bt_conn * conn;                                                   /*######%Ab*/
	struct bt_gatt_read_container* container;
	int _result;                                                             /*######@cI*/
	size_t handle_count;

	conn = bt_rpc_decode_bt_conn(_value);                                    /*####%CtNb*/
	handle_count = ser_decode_uint(_value);
	container = k_malloc(sizeof(struct bt_gatt_read_container) +
			     sizeof(container->params.multiple.handles[0]) * handle_count);
	if (container == NULL) {
		ser_decoding_done_and_check(_value);
		goto alloc_error;
	}
	container->params.handle_count = handle_count;
	container->params.by_uuid.uuid = &container->uuid;
	container->params.multiple.handles = container->handles;

	bt_gatt_read_params_dec(_value, &container->params);                                /*#####@Pk4*/
	container->remote_pointer = ser_decode_uint(_value);
	container->params.func = bt_gatt_read_callback;

	if (!ser_decoding_done_and_check(_value)) {                              /*######%FE*/
		goto decoding_error;                                             /*######QTM*/
	}                                                                        /*######@1Y*/

	_result = bt_gatt_read(conn, &container->params);                                   /*##DlVxcOc*/

	if (_result < 0) {
		k_free(container);
	}

	ser_rsp_send_int(_result);                                               /*##BPC96+4*/

	return;                                                                  /*######%FV*/

decoding_error:
	k_free(container);
alloc_error:
	report_decoding_error(BT_GATT_READ_RPC_CMD, _handler_data);              /*######@LQ*/

}                                                                                /*##B9ELNqo*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_read, BT_GATT_READ_RPC_CMD,         /*####%BlEa*/
	bt_gatt_read_rpc_handler, NULL);                                         /*#####@3ew*/

/*--------------- bt_gatt_write ---------------*/

struct bt_gatt_write_container {
	struct bt_gatt_write_params params;
	uintptr_t remote_pointer;
	uint8_t data[0];
};

void bt_gatt_write_callback(struct bt_conn *conn, uint8_t err,
			    struct bt_gatt_write_params *params)
{
	struct bt_gatt_write_container* container;
	struct nrf_rpc_cbor_ctx _ctx;

	container = CONTAINER_OF(params, struct bt_gatt_write_container, params);

	NRF_RPC_CBOR_ALLOC(_ctx, 3 + 2 + 5);

	bt_rpc_encode_bt_conn(&_ctx.encoder, conn);
	ser_encode_uint(&_ctx.encoder, err);
	ser_encode_uint(&_ctx.encoder, container->remote_pointer);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_WRITE_CALLBACK_RPC_CMD,
		&_ctx, ser_rsp_decode_void, NULL);

	k_free(container);
}

static void bt_gatt_write_rpc_handler(CborValue *_value, void *_handler_data)     /*####%BpLH*/
{                                                                                /*#####@IVE*/

	struct bt_conn * conn;                                                   /*######%Ab*/
	struct bt_gatt_write_container* container;
	int _result;                                                             /*######@cI*/
	size_t buffer_length;

	conn = bt_rpc_decode_bt_conn(_value);                                    /*####%CtNb*/
	buffer_length = ser_decode_buffer_size(_value);
	container = k_malloc(sizeof(struct bt_gatt_write_container) + buffer_length);
	if (container == NULL) {
		ser_decoding_done_and_check(_value);
		goto alloc_error;
	}
	container->params.data = container->data;
	container->params.length = buffer_length;
	ser_decode_buffer(_value, (void *)container->params.data, container->params.length);
	container->params.handle = ser_decode_uint(_value);
	container->params.offset = ser_decode_uint(_value);
	container->remote_pointer = ser_decode_uint(_value);
	container->params.func = bt_gatt_write_callback;

	if (!ser_decoding_done_and_check(_value)) {                              /*######%FE*/
		goto decoding_error;                                             /*######QTM*/
	}                                                                        /*######@1Y*/

	_result = bt_gatt_write(conn, &container->params);                                   /*##DlVxcOc*/

	if (_result < 0) {
		k_free(container);
	}

	ser_rsp_send_int(_result);                                               /*##BPC96+4*/

	return;                                                                  /*######%FV*/

decoding_error:
	k_free(container);
alloc_error:
	report_decoding_error(BT_GATT_WRITE_RPC_CMD, _handler_data);              /*######@LQ*/

}                                                                                /*##B9ELNqo*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_write, BT_GATT_WRITE_RPC_CMD,         /*####%BlEa*/
	bt_gatt_write_rpc_handler, NULL);                                         /*#####@3ew*/

static void bt_gatt_write_without_response_cb_rpc_handler(CborValue *_value, void *_handler_data)      /*####%BvNu*/
{                                                                                                      /*#####@Ubk*/

	struct bt_conn * conn;                                                                         /*########%*/
	uint16_t handle;                                                                               /*########A*/
	uint16_t length;                                                                               /*########Q*/
	uint8_t* data;                                                                                 /*########G*/
	bool sign;                                                                                     /*########v*/
	bt_gatt_complete_func_t func;                                                                  /*########O*/
	void * user_data;                                                                              /*########2*/
	int _result;                                                                                   /*########s*/
	struct ser_scratchpad _scratchpad;                                                             /*########@*/

	SER_SCRATCHPAD_DECLARE(&_scratchpad, _value);                                                  /*##EdQL8vs*/

	conn = bt_rpc_decode_bt_conn(_value);                                                          /*#######%C*/
	handle = ser_decode_uint(_value);                                                              /*#######ip*/
	length = ser_decode_uint(_value);                                                              /*########c*/
	data = ser_decode_buffer_into_scratchpad(&_scratchpad);                                        /*########f*/
	sign = ser_decode_bool(_value);                                                                /*########h*/
	func = (bt_gatt_complete_func_t)ser_decode_callback(_value, bt_gatt_complete_func_t_encoder);  /*########U*/
	user_data = (void *)ser_decode_uint(_value);                                                   /*########@*/

	if (!ser_decoding_done_and_check(_value)) {                                                    /*######%FE*/
		goto decoding_error;                                                                   /*######QTM*/
	}                                                                                              /*######@1Y*/

	_result = bt_gatt_write_without_response_cb(conn, handle, data, length, sign, func, user_data);/*##DhAaHQo*/

	ser_rsp_send_int(_result);                                                                     /*##BPC96+4*/

	return;                                                                                        /*######%FV*/
decoding_error:                                                                                        /*######r6G*/
	report_decoding_error(BT_GATT_WRITE_WITHOUT_RESPONSE_CB_RPC_CMD, _handler_data);               /*######@tA*/

}                                                                                                      /*##B9ELNqo*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_write_without_response_cb, BT_GATT_WRITE_WITHOUT_RESPONSE_CB_RPC_CMD,/*####%Bs2D*/
	bt_gatt_write_without_response_cb_rpc_handler, NULL);                                                     /*#####@5AU*/

struct bt_gatt_subscribe_container {
	struct bt_gatt_subscribe_params params;
	uintptr_t remote_pointer;
	sys_snode_t node;
};

sys_slist_t subscribe_containers = SYS_SLIST_STATIC_INIT(&subscribe_containers);

K_MUTEX_DEFINE(subscribe_containers_mutex);

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
	printk("------NOTIFY %p - %d   %p\n", params, length, data);

	struct nrf_rpc_cbor_ctx _ctx;                                                /*#######%A*/
	size_t _data_size;                                                           /*#######Th*/
	uint8_t _result;                                                             /*#######jm*/
	size_t _scratchpad_size = 0;                                                 /*#######UQ*/
	size_t _buffer_size_max = 21;                                                /*########@*/
	struct bt_gatt_subscribe_container *container;

	container = CONTAINER_OF(params, struct bt_gatt_subscribe_container, params);

	_data_size = sizeof(uint8_t) * length;                                       /*####%CFnV*/
	_buffer_size_max += _data_size;                                              /*#####@o9g*/

	_scratchpad_size += SCRATCHPAD_ALIGN(_data_size);                            /*##EImeShE*/

	NRF_RPC_CBOR_ALLOC(_ctx, _buffer_size_max);                                  /*####%AoDN*/
	ser_encode_uint(&_ctx.encoder, _scratchpad_size);                            /*#####@BNc*/

	bt_rpc_encode_bt_conn(&_ctx.encoder, conn);                                  /*######%A4*/
	ser_encode_uint(&_ctx.encoder, container->remote_pointer);                                      /*#######yr*/
	ser_encode_buffer(&_ctx.encoder, data, _data_size);                          /*#######@E*/

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_SUBSCRIBE_PARAMS_NOTIFY_RPC_CMD,/*####%BN2V*/
		&_ctx, ser_rsp_decode_u8, &_result);                                 /*#####@mEA*/

	return _result;                                                              /*##BX7TDLc*/
}

static inline void bt_gatt_subscribe_params_write(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params,
						    uint32_t callback_slot)
{
	printk("------WRITE %p %d %p %d\n", conn, err, params, callback_slot);
	struct nrf_rpc_cbor_ctx _ctx;                                               /*######%AU*/
	size_t _params_size;                                                        /*#######QP*/
	size_t _scratchpad_size = 0;                                                /*#######4L*/
	size_t _buffer_size_max = 26;                                               /*#######@M*/

	if (params != NULL) {
		_params_size = sizeof(uint8_t) * params->length;                             /*####%CJK7*/
		_buffer_size_max += _params_size;                                           /*#####@Yjk*/
		_scratchpad_size += SCRATCHPAD_ALIGN(_params_size);                         /*##EGWNEUQ*/
	}

	printk("# %d\n", _buffer_size_max);

	NRF_RPC_CBOR_ALLOC(_ctx, _buffer_size_max);                                 /*####%AoDN*/
	printk("#\n");
	ser_encode_uint(&_ctx.encoder, _scratchpad_size);                           /*#####@BNc*/

	printk("#\n");
	bt_rpc_encode_bt_conn(&_ctx.encoder, conn);                                 /*######%A3*/
	printk("#\n");
	ser_encode_uint(&_ctx.encoder, err);                                        /*#######5d*/
	printk("#\n");
	if (params != NULL) {
		ser_encode_uint(&_ctx.encoder, params->handle);                                      /*######@uQ*/
		ser_encode_uint(&_ctx.encoder, params->offset);                                      /*######@uQ*/
		ser_encode_buffer(&_ctx.encoder, params->data, params->length);                                      /*######@uQ*/
	} else {
		ser_encode_null(&_ctx.encoder);
	}
	printk("#\n");
	ser_encode_uint(&_ctx.encoder, callback_slot);                                      /*######@uQ*/

	printk("------SEND\n");
	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_SUBSCRIBE_PARAMS_WRITE_RPC_CMD,/*####%BFre*/
		&_ctx, ser_rsp_decode_void, NULL);                                  /*#####@zdo*/
}

CBKPROXY_HANDLER(bt_gatt_subscribe_params_write_encoder, bt_gatt_subscribe_params_write,
		 (struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params), (conn, err, params));

void bt_gatt_subscribe_params_dec(CborValue *_value, struct bt_gatt_subscribe_params *_data)              /*####%BsZ+*/
{                                                                                                         /*#####@Zf0*/
	_data->notify = ser_decode_bool(_value) ? bt_gatt_subscribe_params_notify : NULL;
	_data->write = (bt_gatt_write_func_t)ser_decode_callback(_value, bt_gatt_subscribe_params_write_encoder);  /*########U*/
	_data->value_handle = ser_decode_uint(_value);                                                    /*#######tU*/
	_data->ccc_handle = ser_decode_uint(_value);                                                      /*########s*/
	_data->value = ser_decode_uint(_value);                                                           /*########w*/
	atomic_set(_data->flags, (atomic_val_t)ser_decode_uint(_value));
}                                                                                                         /*##B9ELNqo*/

static void bt_gatt_subscribe_rpc_handler(CborValue *_value, void *_handler_data)/*####%Bgbl*/
{                                                                                /*#####@4r8*/

	struct bt_conn * conn;                                                   /*######%AX*/
	struct bt_gatt_subscribe_container *container;
	bool new_container = true;
	int _result;                                                             /*######@ZA*/
	uintptr_t remote_pointer;

	conn = bt_rpc_decode_bt_conn(_value);                                    /*####%CuAY*/
	remote_pointer = ser_decode_uint(_value);
	container = get_subscribe_container(remote_pointer, &new_container);
	if (container == NULL) {
		ser_decoding_done_and_check(_value);
		goto alloc_error;
	}
	bt_gatt_subscribe_params_dec(_value, &container->params);

	if (!ser_decoding_done_and_check(_value)) {                              /*######%FE*/
		goto decoding_error;                                             /*######QTM*/
	}                                                                        /*######@1Y*/
	printk("+ notify       %p\n", container->params.notify);
	printk("+ write        %p\n", container->params.write);
	printk("+ value_handle %d\n", container->params.value_handle);
	printk("+ ccc_handle   %d\n", container->params.ccc_handle);
	printk("+ value        %d\n", container->params.value);
	printk("+ flags        %x\n", (uint32_t)atomic_get(container->params.flags));

	_result = bt_gatt_subscribe(conn, &container->params);                              /*##DtSaIiU*/

	ser_rsp_send_int(_result);                                               /*##BPC96+4*/

	if (_result < 0 && new_container) {
		free_subscribe_container(container);
	}

	return;                                                                  /*######%FS*/
decoding_error:                                                                  /*######6vX*/
	if (new_container) {
		free_subscribe_container(container);
	}
alloc_error:
	report_decoding_error(BT_GATT_SUBSCRIBE_RPC_CMD, _handler_data);         /*######@DI*/

}                                                                                /*##B9ELNqo*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_subscribe, BT_GATT_SUBSCRIBE_RPC_CMD,/*####%BsIC*/
	bt_gatt_subscribe_rpc_handler, NULL);                                     /*#####@Ghg*/

static void bt_gatt_resubscribe_rpc_handler(CborValue *_value, void *_handler_data)/*####%BkOL*/
{                                                                                  /*#####@Xt0*/

	uint8_t id;                                                                /*######%Ad*/
	bt_addr_le_t _peer_data;                                                   /*#######Qq*/
	const bt_addr_le_t * peer;                                                 /*#######xp*/
	int _result;                                                               /*#######@o*/
	uintptr_t remote_pointer;
	struct bt_gatt_subscribe_container *container;
	bool new_container = true;

	id = ser_decode_uint(_value);                                              /*####%CrMZ*/
	peer = ser_decode_buffer(_value, &_peer_data, sizeof(bt_addr_le_t));       /*#####@6MU*/
	remote_pointer = ser_decode_uint(_value);
	container = get_subscribe_container(remote_pointer, &new_container);
	if (container == NULL) {
		ser_decoding_done_and_check(_value);
		goto alloc_error;
	}
	bt_gatt_subscribe_params_dec(_value, &container->params);

	if (!ser_decoding_done_and_check(_value)) {                                /*######%FE*/
		goto decoding_error;                                               /*######QTM*/
	}                                                                          /*######@1Y*/

	_result = bt_gatt_resubscribe(id, peer, &container->params);                                   /*##DnPy/2A*/

	ser_rsp_send_int(_result);                                                 /*##BPC96+4*/

	if (_result < 0 && new_container) {
		free_subscribe_container(container);
	}

	return;                                                                    /*######%Ff*/
decoding_error:                                                                    /*######cBP*/
	if (new_container) {
		free_subscribe_container(container);
	}
alloc_error:
	report_decoding_error(BT_GATT_RESUBSCRIBE_RPC_CMD, _handler_data);         /*######@wk*/

}                                                                                  /*##B9ELNqo*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_resubscribe, BT_GATT_RESUBSCRIBE_RPC_CMD,/*####%Bsd0*/
	bt_gatt_resubscribe_rpc_handler, NULL);                                       /*#####@Gj0*/

static void bt_gatt_unsubscribe_rpc_handler(CborValue *_value, void *_handler_data)/*####%BghS*/
{                                                                                  /*#####@rHk*/

	struct bt_conn * conn;                                                     /*######%Ac*/
	int _result;                                                               /*######@CQ*/
	uintptr_t remote_pointer;
	struct bt_gatt_subscribe_container *container;
	bool new_container = false;
	
	conn = bt_rpc_decode_bt_conn(_value);                                      /*####%CmfJ*/
	remote_pointer = ser_decode_uint(_value);                                          /*#####@ETU*/

	if (!ser_decoding_done_and_check(_value)) {                                /*######%FE*/
		goto decoding_error;                                               /*######QTM*/
	}                                                                          /*######@1Y*/

	container = get_subscribe_container(remote_pointer, &new_container);

	if (container != NULL) {
		_result = bt_gatt_unsubscribe(conn, &container->params);                               /*##DorqYnE*/
		free_subscribe_container(container);
	} else {
		_result = -EINVAL;
	}

	ser_rsp_send_int(_result);                                                 /*##BPC96+4*/

	return;                                                                    /*######%FT*/
decoding_error:                                                                    /*######mA0*/
	report_decoding_error(BT_GATT_UNSUBSCRIBE_RPC_CMD, _handler_data);         /*######@IQ*/

}                                                                                  /*##B9ELNqo*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_unsubscribe, BT_GATT_UNSUBSCRIBE_RPC_CMD,/*####%Bpx/*/
	bt_gatt_unsubscribe_rpc_handler, NULL);                                       /*#####@k3Y*/

static void bt_rpc_gatt_subscribe_flag_update_rpc_handler(CborValue *_value, void *_handler_data)/*####%BsDx*/
{                                                                                             /*#####@n1k*/
	uint32_t flags_bit;                                                                   /*######h7r*/
	int _result;                                                                          /*######@xM*/
	uintptr_t remote_pointer;
	struct bt_gatt_subscribe_container *container;
	bool new_container = false;
	int value;

	remote_pointer = ser_decode_uint(_value);                                                     /*####%CkaJ*/
	flags_bit = ser_decode_uint(_value);                                                  /*#####@UZ0*/
	value = ser_decode_int(_value);                                                  /*#####@UZ0*/

	if (!ser_decoding_done_and_check(_value)) {                                           /*######%FE*/
		goto decoding_error;                                                          /*######QTM*/
	}                                                                                     /*######@1Y*/

	container = get_subscribe_container(remote_pointer, &new_container);
	if (container == NULL) {
		_result = -EINVAL;
	} else {
		if (atomic_test_bit(container->params.flags, flags_bit)) {
			_result = 1;
		} else {
			_result = 0;
		}

		if (value == 0) {
			atomic_clear_bit(container->params.flags, flags_bit);
		} else if (value > 0) {
			atomic_set_bit(container->params.flags, flags_bit);
		}
	}

	ser_rsp_send_int(_result);                                                            /*##BPC96+4*/

	return;                                                                               /*######%FT*/
decoding_error:                                                                               /*######u8s*/
	report_decoding_error(BT_RPC_GATT_SUBSCRIBE_FLAG_UPDATE_RPC_CMD, _handler_data);         /*######@28*/

}                                                                                             /*##B9ELNqo*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_rpc_gatt_subscribe_flag_update, BT_RPC_GATT_SUBSCRIBE_FLAG_UPDATE_RPC_CMD,/*####%Bge0*/
	bt_rpc_gatt_subscribe_flag_update_rpc_handler, NULL);                                                  /*#####@Rfc*/

