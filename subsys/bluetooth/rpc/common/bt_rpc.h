/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BT_RPC_H_
#define BT_RPC_H_

#include "bluetooth/gatt.h"

int bt_rpc_gatt_subscribe_flag_set(struct bt_gatt_subscribe_params *params, uint32_t flags_bit);

int bt_rpc_gatt_subscribe_flag_clear(struct bt_gatt_subscribe_params *params, uint32_t flags_bit);

int bt_rpc_gatt_subscribe_flag_get(struct bt_gatt_subscribe_params *params, uint32_t flags_bit);

#endif /* BT_RPC_H_ */
