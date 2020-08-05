
#include "zephyr.h"

#include "nrf_rpc_cbor.h"

#include "bluetooth/bluetooth.h"

#include "bt_rpc_common.h"
#include "serialize.h"


void report_decoding_error(uint8_t cmd_evt_id, void* DATA) {
	nrf_rpc_err(-EBADMSG, NRF_RPC_ERR_SRC_RECV, &bt_rpc_grp, cmd_evt_id,
		    NRF_RPC_PACKET_TYPE_CMD);
}


void bt_ready_cb(int err)
{
	printk("BT READY: %d\n", err);
}


static void bt_enable_rpc_handler(CborValue *_value, void *_handler_data)        /*####%Bims*/
{                                                                                /*#####@U54*/

	bt_ready_cb_t cb;                                                        /*####%AX3q*/
	int _result;                                                             /*#####@ImM*/

	cb = bt_ready_cb;  /*##Ctv5nEY*/

	if (!ser_decoding_done_and_check(_value)) {                              /*######%AE*/
		goto decoding_error;                                             /*######QTM*/
	}                                                                        /*######@1Y*/

	_result = bt_enable(cb);                                                 /*##DqdsuHg*/

	ser_rsp_send_i32(_result);                                               /*##BNedR2Y*/

	return;                                                                  /*######%B+*/
decoding_error:                                                                  /*#######XK*/
	report_decoding_error(BT_ENABLE_RPC_CMD, _handler_data);                 /*#######Ex*/
}                                                                                /*#######@I*/

NRF_RPC_CBOR_CMD_DECODER(bt_rpc_grp, bt_enable, BT_ENABLE_RPC_CMD,               /*####%BmEu*/
	bt_enable_rpc_handler, NULL);                                            /*#####@rg4*/
