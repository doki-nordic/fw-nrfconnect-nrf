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

#if defined(CONFIG_BT_GATT_AUTO_DISCOVER_CCC)
#error "CONFIG_BT_GATT_AUTO_DISCOVER_CCC is not supported by the RPC GATT"
#endif

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
	return bt_uuid_enc(NULL, _data->uuid) + 3 + 3 + 3;
}

static void bt_gatt_discover_params_enc(CborEncoder *_encoder,
	const struct bt_gatt_discover_params *_data)
{
	bt_uuid_enc(_encoder, _data->uuid);
	ser_encode_uint(_encoder, _data->start_handle);
	ser_encode_uint(_encoder, _data->end_handle);
	ser_encode_uint(_encoder, _data->type);
}


int bt_gatt_discover(struct bt_conn *conn, struct bt_gatt_discover_params *params)
{
	struct nrf_rpc_cbor_ctx _ctx;
	int _result;

	NRF_RPC_CBOR_ALLOC(_ctx, 3 + bt_gatt_discover_params_buf_size(params) + 5);

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

	conn = bt_rpc_decode_bt_conn(_value);
	params_pointer = ser_decode_uint(_value);
	params = (struct bt_gatt_discover_params *)params_pointer;

	if (ser_decode_is_null(_value)) {
		ser_decode_skip(_value);
		attr = NULL;
	} else {
		attr->uuid = bt_uuid_dec(_value, &uuid_buffers[0].uuid);
		attr->handle = ser_decode_uint(_value);
		attr_uuid_16 = (struct bt_uuid_16 *)attr->uuid;
		if (ser_decode_is_null(_value)) {
			ser_decode_skip(_value);
			attr->user_data = NULL;
		} else if (attr->uuid == NULL || attr->uuid->type != BT_UUID_TYPE_16) {
			LOG_ERR("Invalid attribute UUID");
			goto decoding_done_with_error;
		} else if (attr_uuid_16->val == BT_UUID_GATT_PRIMARY_VAL ||
			attr_uuid_16->val == BT_UUID_GATT_SECONDARY_VAL) {
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
			LOG_ERR("Unsupported attribute UUID");
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


/*--------------- bt_gatt_read ---------------*/

static size_t bt_gatt_read_params_buf_size(const struct bt_gatt_read_params *_data)
{
	size_t size = 5 + 5;
	if (_data->handle_count == 0) {
		size += 3 + 3 + bt_uuid_enc(NULL, _data->by_uuid.uuid);
	} else if (_data->handle_count == 1) {
		size += 3 + 3;
	} else {
		size += 5 + sizeof(_data->multiple.handles[0]) * _data->handle_count + 1;
	}
	return size;
}

static void bt_gatt_read_params_enc(CborEncoder *_encoder,
	const struct bt_gatt_read_params *_data)
{
	SERIALIZE(CUSTOM_STRUCT(struct bt_gatt_read_params));
	ser_encode_uint(_encoder, _data->handle_count);
	if (_data->handle_count == 0) {
		ser_encode_uint(_encoder, _data->by_uuid.start_handle);
		ser_encode_uint(_encoder, _data->by_uuid.end_handle);
		bt_uuid_enc(_encoder, _data->by_uuid.uuid);
	} else if (_data->handle_count == 1) {
		ser_encode_uint(_encoder, _data->single.handle);
		ser_encode_uint(_encoder, _data->single.offset);
	} else {
		ser_encode_buffer(_encoder, _data->multiple.handles,
				  sizeof(_data->multiple.handles[0]) * _data->handle_count);
		ser_encode_bool(_encoder, _data->multiple.variable);
	}
}

int bt_gatt_read(struct bt_conn *conn, struct bt_gatt_read_params *params)
{
	struct nrf_rpc_cbor_ctx _ctx;                                            /*######%AR*/
	int _result;                                                             /*######RDP*/

	NRF_RPC_CBOR_ALLOC(_ctx, 3 + bt_gatt_read_params_buf_size(params) + 5);

	bt_rpc_encode_bt_conn(&_ctx.encoder, conn);                              /*####%A6Js*/
	bt_gatt_read_params_enc(&_ctx.encoder, params);                          /*#####@lMw*/
	ser_encode_uint(&_ctx.encoder, (uintptr_t)params);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_READ_RPC_CMD,               /*####%BJ5t*/
		&_ctx, ser_rsp_decode_i32, &_result);                            /*#####@OCM*/

	return _result;                                                          /*##BX7TDLc*/
}

static void bt_gatt_read_callback_rpc_handler(CborValue *_value, void *_handler_data)/*####%BhJO*/
{                                                                                    /*#####@mCI*/
	struct ser_scratchpad scratchpad;
	struct bt_conn *conn;
	uintptr_t params_pointer;
	uint8_t err;
	uint8_t result;
	struct bt_gatt_read_params *params;
	void *data;
	uint16_t length;

	SER_SCRATCHPAD_DECLARE(&scratchpad, _value);

	conn = bt_rpc_decode_bt_conn(_value);
	err = ser_decode_uint(_value);
	params_pointer = ser_decode_uint(_value);
	params = (struct bt_gatt_read_params *)params_pointer;

	length = ser_decode_buffer_size(_value);
	data = ser_decode_buffer_into_scratchpad(&scratchpad);

	if (!ser_decoding_done_and_check(_value)) {
		goto decoding_error;
	}

	result = params->func(conn, err, params, data, length);

	ser_rsp_send_uint(result);

	return;

decoding_error:
	report_decoding_error(BT_GATT_READ_CALLBACK_RPC_CMD, _handler_data);

}                                                                                    /*##B9ELNqo*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_read_callback, BT_GATT_READ_CALLBACK_RPC_CMD,/*####%Bvya*/
	bt_gatt_read_callback_rpc_handler, NULL);                                         /*#####@vOM*/

/*--------------- bt_gatt_write ---------------*/

static size_t bt_gatt_write_params_buf_size(const struct bt_gatt_write_params *_data)
{
	return 5 + _data->length + 3 + 3;
}

static void bt_gatt_write_params_enc(CborEncoder *_encoder,
	const struct bt_gatt_write_params *_data)
{
	ser_encode_buffer(_encoder, _data->data, _data->length);
	ser_encode_uint(_encoder, _data->handle);
	ser_encode_uint(_encoder, _data->offset);
}

int bt_gatt_write(struct bt_conn *conn, struct bt_gatt_write_params *params)
{
	struct nrf_rpc_cbor_ctx _ctx;                                            /*######%AR*/
	int _result;                                                             /*######RDP*/

	NRF_RPC_CBOR_ALLOC(_ctx, 3 + bt_gatt_write_params_buf_size(params) + 5);

	bt_rpc_encode_bt_conn(&_ctx.encoder, conn);                              /*####%A6Js*/
	bt_gatt_write_params_enc(&_ctx.encoder, params);                          /*#####@lMw*/
	ser_encode_uint(&_ctx.encoder, (uintptr_t)params);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_WRITE_RPC_CMD,               /*####%BJ5t*/
		&_ctx, ser_rsp_decode_i32, &_result);                            /*#####@OCM*/

	return _result;                                                          /*##BX7TDLc*/
}

static void bt_gatt_write_callback_rpc_handler(CborValue *_value, void *_handler_data)/*####%BhJO*/
{                                                                                    /*#####@mCI*/
	struct bt_conn *conn;
	uint8_t err;
	struct bt_gatt_write_params *params;

	conn = bt_rpc_decode_bt_conn(_value);
	err = ser_decode_uint(_value);
	params = (struct bt_gatt_write_params *)ser_decode_uint(_value);

	if (!ser_decoding_done_and_check(_value)) {
		goto decoding_error;
	}

	params->func(conn, err, params);

	ser_rsp_send_void();

	return;

decoding_error:
	report_decoding_error(BT_GATT_WRITE_CALLBACK_RPC_CMD, _handler_data);

}                                                                                    /*##B9ELNqo*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_write_callback, BT_GATT_WRITE_CALLBACK_RPC_CMD,/*####%Bvya*/
	bt_gatt_write_callback_rpc_handler, NULL);                                         /*#####@vOM*/


static void bt_gatt_complete_func_t_callback_rpc_handler(CborValue *_value, void *_handler_data)/*####%BoX7*/
{                                                                                               /*#####@SHA*/

	struct bt_conn * conn;                                                                  /*######%Ac*/
	void * user_data;                                                                       /*######9nB*/
	bt_gatt_complete_func_t callback_slot;                                                  /*######@Qo*/

	conn = bt_rpc_decode_bt_conn(_value);                                                   /*######%Cs*/
	user_data = (void *)ser_decode_uint(_value);                                            /*######uWh*/
	callback_slot = (bt_gatt_complete_func_t)ser_decode_callback_call(_value);              /*######@uE*/

	if (!ser_decoding_done_and_check(_value)) {                                             /*######%FE*/
		goto decoding_error;                                                            /*######QTM*/
	}                                                                                       /*######@1Y*/

	callback_slot(conn, user_data);                                                         /*##Dvus85Q*/

	ser_rsp_send_void();                                                                    /*##BEYGLxw*/

	return;                                                                                 /*######%Fd*/
decoding_error:                                                                                 /*######Mf3*/
	report_decoding_error(BT_GATT_COMPLETE_FUNC_T_CALLBACK_RPC_CMD, _handler_data);         /*######@xE*/

}                                                                                               /*##B9ELNqo*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_complete_func_t_callback, BT_GATT_COMPLETE_FUNC_T_CALLBACK_RPC_CMD,/*####%BsLA*/
	bt_gatt_complete_func_t_callback_rpc_handler, NULL);                                                    /*#####@aQ8*/

int bt_gatt_write_without_response_cb(struct bt_conn *conn, uint16_t handle,
				      const void *data, uint16_t length,
				      bool sign, bt_gatt_complete_func_t func,
				      void *user_data)
{
	SERIALIZE(TYPE(data, uint8_t*));
	SERIALIZE(SIZE_PARAM(data, length));

	struct nrf_rpc_cbor_ctx _ctx;                                                  /*#######%A*/
	size_t _data_size;                                                             /*#######U2*/
	int _result;                                                                   /*#######aX*/
	size_t _scratchpad_size = 0;                                                   /*#######f8*/
	size_t _buffer_size_max = 30;                                                  /*########@*/

	_data_size = sizeof(uint8_t) * length;                                         /*####%CFnV*/
	_buffer_size_max += _data_size;                                                /*#####@o9g*/

	_scratchpad_size += SCRATCHPAD_ALIGN(_data_size);                              /*##EImeShE*/

	NRF_RPC_CBOR_ALLOC(_ctx, _buffer_size_max);                                    /*####%AoDN*/
	ser_encode_uint(&_ctx.encoder, _scratchpad_size);                              /*#####@BNc*/

	bt_rpc_encode_bt_conn(&_ctx.encoder, conn);                                    /*#######%A*/
	ser_encode_uint(&_ctx.encoder, handle);                                        /*#######yN*/
	ser_encode_uint(&_ctx.encoder, length);                                        /*########r*/
	ser_encode_buffer(&_ctx.encoder, data, _data_size);                            /*########C*/
	ser_encode_bool(&_ctx.encoder, sign);                                          /*########6*/
	ser_encode_callback(&_ctx.encoder, func);                                      /*########M*/
	ser_encode_uint(&_ctx.encoder, (uintptr_t)user_data);                          /*########@*/

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_WRITE_WITHOUT_RESPONSE_CB_RPC_CMD,/*####%BKC9*/
		&_ctx, ser_rsp_decode_i32, &_result);                                  /*#####@1/w*/

	return _result;                                                                /*##BX7TDLc*/
}


int bt_gatt_subscribe(struct bt_conn *conn,
		      struct bt_gatt_subscribe_params *params)
{
	SERIALIZE();
}
