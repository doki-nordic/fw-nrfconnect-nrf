
__attribute__((used))
static size_t bt_uuid_sp_size(const struct bt_uuid *uuid)
{
	switch (uuid->type) {
	case BT_UUID_TYPE_16:
		return sizeof(struct bt_uuid_16);

	case BT_UUID_TYPE_32:
		return sizeof(struct bt_uuid_32);

	case BT_UUID_TYPE_128:
		return sizeof(struct bt_uuid_128);

	default:
		return 0;
	}
}

static size_t bt_uuid_buf_size(const struct bt_uuid *uuid)
{
	switch (uuid->type) {
	case BT_UUID_TYPE_16:
		return 2 + sizeof(struct bt_uuid_16);

	case BT_UUID_TYPE_32:
		return 2 + sizeof(struct bt_uuid_32);

	case BT_UUID_TYPE_128:
		return 2 + sizeof(struct bt_uuid_128);

	default:
		return 0;
	}
}



size_t bt_gatt_write_params_sp_size(const struct bt_gatt_write_params *_data)    /*####%BoNq*/
{                                                                                /*#####@LTY*/

	size_t _scratchpad_size = 0;                                             /*##ATz5YrA*/

	_scratchpad_size += SCRATCHPAD_ALIGN(sizeof(uint8_t) * _data->length);   /*##EOuNfh8*/

	return _scratchpad_size;                                                 /*##BRWAmyU*/

}                                                                                /*##B9ELNqo*/

size_t bt_gatt_write_params_buf_size(const struct bt_gatt_write_params *_data)   /*####%BnIf*/
{                                                                                /*#####@ctg*/

	size_t _buffer_size_max = 19;                                            /*##AeoYUDI*/

	_buffer_size_max += sizeof(uint8_t) * _data->length;                     /*##CHa/F4k*/

	return _buffer_size_max;                                                 /*##BWmN6G8*/

}                                                                                /*##B9ELNqo*/

void bt_gatt_write_params_enc(CborEncoder *_encoder, const struct bt_gatt_write_params *_data)/*####%BuSM*/
{                                                                                             /*#####@RHw*/

	SERIALIZE(STRUCT(struct bt_gatt_write_params));
	SERIALIZE(TYPE(data, uint8_t*));
	SERIALIZE(TYPE(func, void*));
	SERIALIZE(SIZE_PARAM(data, length));

	ser_encode_uint(_encoder, (uintptr_t)_data->func);                                    /*#######%A*/
	ser_encode_uint(_encoder, _data->handle);                                             /*#######9Q*/
	ser_encode_uint(_encoder, _data->offset);                                             /*#######Du*/
	ser_encode_uint(_encoder, _data->length);                                             /*#######q8*/
	ser_encode_buffer(_encoder, _data->data, sizeof(uint8_t) * _data->length);            /*########@*/

}                                                                                             /*##B9ELNqo*/


int bt_gatt_write(struct bt_conn *conn, struct bt_gatt_write_params *params)
{
	struct nrf_rpc_cbor_ctx _ctx;                                            /*######%Af*/
	int _result;                                                             /*#######LL*/
	size_t _scratchpad_size = 0;                                             /*#######kq*/
	size_t _buffer_size_max = 13;                                            /*#######@k*/

	_buffer_size_max += bt_gatt_write_params_buf_size(params);               /*##CF9g2iw*/

	_scratchpad_size += bt_gatt_write_params_sp_size(params);                /*##EJRho8w*/

	NRF_RPC_CBOR_ALLOC(_ctx, _buffer_size_max);                              /*####%AoDN*/
	ser_encode_uint(&_ctx.encoder, _scratchpad_size);                        /*#####@BNc*/

	bt_rpc_encode_bt_conn(&_ctx.encoder, conn);                              /*####%A4q0*/
	bt_gatt_write_params_enc(&_ctx.encoder, params);                         /*#####@MS0*/
	ser_encode_uint(&_ctx.encoder, (uintptr_t)params);

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_WRITE_RPC_CMD,              /*####%BP9I*/
		&_ctx, ser_rsp_decode_i32, &_result);                            /*#####@HYE*/

	return _result;                                                          /*##BX7TDLc*/
}


static void bt_gatt_write_func_rpc_send_rpc_handler(CborValue *_value, void *_handler_data)/*####%Bqt/*/
{                                                                                          /*#####@LiM*/

	struct bt_conn * conn;                                                             /*######%AS*/
	uint8_t err;                                                                       /*######mqz*/
	uintptr_t params_ptr;                                                              /*######@38*/

	struct bt_gatt_write_params *params;

	conn = bt_rpc_decode_bt_conn(_value);                                              /*######%Ct*/
	err = ser_decode_uint(_value);                                                     /*######5nf*/
	params_ptr = ser_decode_uint(_value);                                              /*######@/Q*/

	if (!ser_decoding_done_and_check(_value)) {                                        /*######%FE*/
		goto decoding_error;                                                       /*######QTM*/
	}                                                                                  /*######@1Y*/

	SERIALIZE(CUSTOM_EXECUTE);
	params = (struct bt_gatt_write_params *)params_ptr;
	params->func(conn, err, params);

	ser_rsp_send_void();                                                               /*##BEYGLxw*/

	return;                                                                            /*######%FW*/
decoding_error:                                                                            /*######+eq*/
	report_decoding_error(BT_GATT_WRITE_FUNC_RPC_SEND_RPC_CMD, _handler_data);         /*######@3I*/

}                                                                                          /*##B9ELNqo*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_write_func_rpc_send, BT_GATT_WRITE_FUNC_RPC_SEND_RPC_CMD,/*####%Bj5w*/
	bt_gatt_write_func_rpc_send_rpc_handler, NULL);                                               /*#####@xsE*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#if 0

static inline void bt_gatt_exchange_params_cb_callback(struct bt_conn *conn, uint8_t err,
	struct bt_gatt_exchange_params *params)
{
	SERIALIZE(CALLBACK(bt_gatt_exchange_params_cb));

	struct nrf_rpc_cbor_ctx _ctx;                                                    /*####%ATwe*/
	size_t _buffer_size_max = 10;                                                    /*#####@Gjo*/

	NRF_RPC_CBOR_ALLOC(_ctx, _buffer_size_max);                                      /*##AvrU03s*/

	bt_rpc_encode_bt_conn(&_ctx.encoder, conn);                                      /*######%A9*/
	ser_encode_uint(&_ctx.encoder, err);                                             /*######4EY*/
	ser_encode_uint(&_ctx.encoder, (uintptr_t)params);                               /*######@7k*/

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_EXCHANGE_PARAMS_CB_CALLBACK_RPC_CMD,/*####%BP2h*/
		&_ctx, ser_rsp_decode_void, NULL);                                       /*#####@Hss*/
}


//CBKPROXY_HANDLER(bt_ready_cb_t_encoder, bt_ready_cb_t_callback, (int err), (err));

static void bt_gatt_exchange_mtu_rpc_handler(CborValue *_value, void *_handler_data)                       /*####%BqLp*/
{                                                                                                          /*#####@+T4*/

	struct bt_conn * conn;                                                                             /*######%AS*/
	struct bt_gatt_exchange_params * params;                                                           /*#######Tx*/
	bt_gatt_exchange_params_cb func;                                                                   /*#######J3*/
	int _result;                                                                                       /*#######@M*/

	conn = bt_rpc_decode_bt_conn(_value);                                                              /*######%Cv*/
	params = (struct bt_gatt_exchange_params *)ser_decode_uint(_value);                                /*######1bL*/
	func = (bt_gatt_exchange_params_cb)ser_decode_callback(_value, bt_gatt_exchange_params_cb_encoder);/*######@9k*/

	if (!ser_decoding_done_and_check(_value)) {                                                        /*######%FE*/
		goto decoding_error;                                                                       /*######QTM*/
	}                                                                                                  /*######@1Y*/

	_result = bt_gatt_exchange_mtu(conn, params);                                                      /*##DnecMts*/

	ser_rsp_send_int(_result);                                                                         /*##BPC96+4*/

	return;                                                                                            /*######%Fd*/
decoding_error:                                                                                            /*######ZyI*/
	report_decoding_error(BT_GATT_EXCHANGE_MTU_RPC_CMD, _handler_data);                                /*######@WA*/

}                                                                                                          /*##B9ELNqo*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_exchange_mtu, BT_GATT_EXCHANGE_MTU_RPC_CMD,/*####%Bguv*/
	bt_gatt_exchange_mtu_rpc_handler, NULL);                                        /*#####@uIg*/

#endif


void bt_gatt_write_params_dec(struct ser_scratchpad *_scratchpad, struct bt_gatt_write_params *_data)/*####%BiW3*/
{                                                                                                    /*#####@IcY*/

	CborValue *_value = _scratchpad->value;                                                      /*##AU3cSLw*/

	_data->func = (void*)(uintptr_t)ser_decode_uint(_value);                                     /*#######%C*/
	_data->handle = ser_decode_uint(_value);                                                     /*#######v9*/
	_data->offset = ser_decode_uint(_value);                                                     /*#######Gg*/
	_data->length = ser_decode_uint(_value);                                                     /*#######GQ*/
	_data->data = ser_decode_buffer_into_scratchpad(_scratchpad);                                /*########@*/

}                                                                                                    /*##B9ELNqo*/

struct bt_gatt_write_params_container {
	struct bt_gatt_write_params params;
	uintptr_t remote_func;
	uintptr_t remote_pointer;
};

static void bt_gatt_write_func_rpc_send(struct bt_conn *conn, uint8_t err,
	uintptr_t params_ptr)
{
	SERIALIZE();

	struct nrf_rpc_cbor_ctx _ctx;                                            /*####%ATwe*/
	size_t _buffer_size_max = 10;                                            /*#####@Gjo*/

	NRF_RPC_CBOR_ALLOC(_ctx, _buffer_size_max);                              /*##AvrU03s*/

	bt_rpc_encode_bt_conn(&_ctx.encoder, conn);                              /*######%Ay*/
	ser_encode_uint(&_ctx.encoder, err);                                     /*######zmh*/
	ser_encode_uint(&_ctx.encoder, params_ptr);                              /*######@AU*/

	nrf_rpc_cbor_cmd_no_err(&bt_rpc_grp, BT_GATT_WRITE_FUNC_RPC_SEND_RPC_CMD,/*####%BGQM*/
		&_ctx, ser_rsp_decode_void, NULL);                               /*#####@A7M*/
}

static void bt_gatt_write_func_rpc_callback(struct bt_conn *conn, uint8_t err,
	struct bt_gatt_write_params *params)
{
	uintptr_t remote_pointer;
	struct bt_gatt_write_params_container* container;
	
	container = CONTAINER_OF(params, struct bt_gatt_write_params_container,
				 params);
	remote_pointer = container->remote_pointer;
	k_free(container);
	bt_gatt_write_func_rpc_send(conn, err, remote_pointer);
}

static void bt_gatt_write_rpc_handler(CborValue *_value, void *_handler_data)    /*####%Bp0+*/
{                                                                                /*#####@Tus*/

	struct bt_conn * conn;                                                   /*######%AU*/
	struct bt_gatt_write_params_container* container = NULL;
	int _result;                                                             /*#######aY*/
	struct ser_scratchpad _scratchpad;                                       /*#######@U*/

	SER_SCRATCHPAD_DECLARE(&_scratchpad, _value);                            /*##EdQL8vs*/
	
	container = k_malloc(sizeof(struct bt_gatt_write_params_container));
	if (container == NULL) {
		goto alloc_error;                                             /*######QTM*/
	}

	conn = bt_rpc_decode_bt_conn(_value);                                    /*####%CtAR*/
	bt_gatt_write_params_dec(&_scratchpad, &container->params);
	container->remote_pointer = ser_decode_uint(_value);
	container->remote_func = (uintptr_t)container->params.func;
	container->params.func = bt_gatt_write_func_rpc_callback;

	if (!ser_decoding_done_and_check(_value)) {                              /*######%FE*/
		goto decoding_error;                                             /*######QTM*/
	}                                                                        /*######@1Y*/

	_result = bt_gatt_write(conn, &container->params);                                  /*##Djmhdgo*/

	if (_result < 0) {
		k_free(container);
	}

	ser_rsp_send_int(_result);                                               /*##BPC96+4*/

	return;                                                                  /*######%FR*/
decoding_error:                                                                  /*######rBK*/
	k_free(container);
alloc_error:                                                                  /*######rBK*/
	report_decoding_error(BT_GATT_WRITE_RPC_CMD, _handler_data);             /*######@3I*/

}                                                                                /*##B9ELNqo*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_gatt_write, BT_GATT_WRITE_RPC_CMD,       /*####%BsXZ*/
	bt_gatt_write_rpc_handler, NULL);                                        /*#####@rf4*/
