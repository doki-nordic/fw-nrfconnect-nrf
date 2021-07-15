/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#define NRF_RPC_LOG_MODULE NRF_RPC_OS
#include <nrf_rpc_log.h>

#include <nrf_rpc_errno.h>

#include "nrf_rpc_os.h"

/* Maximum number of remote thread that this implementation allows. */
#define MAX_REMOTE_THREADS 255

/* Initial value contains ones (context free) on the
 * CONFIG_NRF_RPC_CMD_CTX_POOL_SIZE most significant bits.
 */
#define CONTEXT_MASK_INIT_VALUE						       \
	(~(((atomic_val_t)1 << (8 * sizeof(atomic_val_t) -		       \
				CONFIG_NRF_RPC_CMD_CTX_POOL_SIZE)) - 1))

struct pool_start_msg {
	const uint8_t *data;
	size_t len;
};

static nrf_rpc_os_work_t thread_pool_callback;

static struct pool_start_msg pool_start_msg_buf[2];
static struct k_msgq pool_start_msg;

static struct k_sem context_reserved;
static atomic_t context_mask;

static uint32_t remote_thread_total;

struct k_sem _nrf_rpc_os_remote_counter;

static K_THREAD_STACK_ARRAY_DEFINE(pool_stacks,
	CONFIG_NRF_RPC_THREAD_POOL_SIZE,
	CONFIG_NRF_RPC_THREAD_STACK_SIZE);

static struct k_thread pool_threads[CONFIG_NRF_RPC_THREAD_POOL_SIZE];

BUILD_ASSERT(CONFIG_NRF_RPC_CMD_CTX_POOL_SIZE > 0,
	     "CONFIG_NRF_RPC_CMD_CTX_POOL_SIZE must be greaten than zero");
BUILD_ASSERT(CONFIG_NRF_RPC_CMD_CTX_POOL_SIZE <= 8 * sizeof(atomic_val_t),
	     "CONFIG_NRF_RPC_CMD_CTX_POOL_SIZE too big");
BUILD_ASSERT(sizeof(uint32_t) == sizeof(atomic_val_t),
	     "Only atomic_val_t is implemented that is the same as uint32_t");

static int shmem_init();

static void thread_pool_entry(void *p1, void *p2, void *p3)
{
	struct pool_start_msg msg;

	do {
		k_msgq_get(&pool_start_msg, &msg, K_FOREVER);
		thread_pool_callback(msg.data, msg.len);
	} while (1);
}

int nrf_rpc_os_init(nrf_rpc_os_work_t callback)
{
	int err;
	int i;

	__ASSERT_NO_MSG(callback != NULL);

	thread_pool_callback = callback;

	err = shmem_init();
	if (err < 0) {
		return err;
	}

	err = k_sem_init(&context_reserved, CONFIG_NRF_RPC_CMD_CTX_POOL_SIZE,
			 CONFIG_NRF_RPC_CMD_CTX_POOL_SIZE);
	if (err < 0) {
		return err;
	}

	err = k_sem_init(&_nrf_rpc_os_remote_counter, 0, MAX_REMOTE_THREADS);
	if (err < 0) {
		return err;
	}
	remote_thread_total = 0;

	atomic_set(&context_mask, CONTEXT_MASK_INIT_VALUE);

	k_msgq_init(&pool_start_msg, (char *)pool_start_msg_buf,
		    sizeof(struct pool_start_msg),
		    ARRAY_SIZE(pool_start_msg_buf));

	for (i = 0; i < CONFIG_NRF_RPC_THREAD_POOL_SIZE; i++) {
		k_thread_create(&pool_threads[i], pool_stacks[i],
			K_THREAD_STACK_SIZEOF(pool_stacks[i]),
			thread_pool_entry,
			NULL, NULL, NULL,
			CONFIG_NRF_RPC_THREAD_PRIORITY, 0, K_NO_WAIT);
	}

	return 0;
}

void nrf_rpc_os_thread_pool_send(const uint8_t *data, size_t len)
{
	struct pool_start_msg msg;

	msg.data = data;
	msg.len = len;
	k_msgq_put(&pool_start_msg, &msg, K_FOREVER);
}

void nrf_rpc_os_msg_set(struct nrf_rpc_os_msg *msg, const uint8_t *data,
			size_t len)
{
	k_sched_lock();
	msg->data = data;
	msg->len = len;
	k_sem_give(&msg->sem);
	k_sched_unlock();
}

void nrf_rpc_os_msg_get(struct nrf_rpc_os_msg *msg, const uint8_t **data,
			size_t *len)
{
	k_sem_take(&msg->sem, K_FOREVER);
	k_sched_lock();
	*data = msg->data;
	*len = msg->len;
	k_sched_unlock();
}

uint32_t nrf_rpc_os_ctx_pool_reserve(void)
{
	uint32_t number;
	atomic_val_t old_mask;
	atomic_val_t new_mask;

	k_sem_take(&context_reserved, K_FOREVER);

	do {
		old_mask = atomic_get(&context_mask);
		number = __CLZ(old_mask);
		new_mask = old_mask & ~(0x80000000u >> number);
	} while (!atomic_cas(&context_mask, old_mask, new_mask));

	return number;
}

void nrf_rpc_os_ctx_pool_release(uint32_t number)
{
	__ASSERT_NO_MSG(number < CONFIG_NRF_RPC_CMD_CTX_POOL_SIZE);

	atomic_or(&context_mask, 0x80000000u >> number);
	k_sem_give(&context_reserved);
}

void nrf_rpc_os_remote_count(int count)
{
	__ASSERT_NO_MSG(count > 0);
	__ASSERT_NO_MSG(count <= MAX_REMOTE_THREADS);

	NRF_RPC_DBG("Remote thread count changed from %d to %d",
		    remote_thread_total, count);

	while ((int)remote_thread_total < count) {
		k_sem_give(&_nrf_rpc_os_remote_counter);
		remote_thread_total++;
	}
	while ((int)remote_thread_total > count) {
		k_sem_take(&_nrf_rpc_os_remote_counter, K_FOREVER);
		remote_thread_total--;
	}
}


#ifdef CONFIG_NRF_RPC_TR_SHMEM

#include <drivers/ipm.h>

#define SHM_NODE            DT_CHOSEN(zephyr_ipc_shm)
#define SHM_START_ADDR      (DT_REG_ADDR(SHM_NODE) + 0x400)
#define SHM_SIZE            (DT_REG_SIZE(SHM_NODE) - 0x400)

void *nrf_rpc_os_out_shmem_ptr;
void *nrf_rpc_os_in_shmem_ptr;

static void (*signal_handler)(void);

static const struct device *ipm_tx_handle;
static const struct device *ipm_rx_handle;

void nrf_rpc_os_signal(void)
{
	int err = ipm_send(ipm_tx_handle, 0, 0, NULL, 0);
	if (err != 0) {
		LOG_ERR("Failed to notify: %d", err);
	}
}

void nrf_rpc_os_signal_handler(void (*handler)(void))
{
	signal_handler = handler;
}

static void ipm_callback(const struct device *ipmdev, void *user_data, uint32_t id,
			 volatile void *data)
{
	if (signal_handler != NULL)
		signal_handler();
}

static int shmem_init()
{
	uint32_t size = (SHM_SIZE / 8) * 4;
	uint32_t addr1 = SHM_START_ADDR;
	uint32_t addr2 = SHM_START_ADDR + size;
	if (IS_ENABLED(CONFIG_NRF_RPC_SHMEM_PRIMARY)) {
		nrf_rpc_os_out_shmem_ptr = (void*)addr1;
		nrf_rpc_os_in_shmem_ptr = (void*)addr2;
	} else {
		nrf_rpc_os_out_shmem_ptr = (void*)addr2;
		nrf_rpc_os_in_shmem_ptr = (void*)addr1;
	}

	/* IPM setup. */
	ipm_tx_handle = device_get_binding(IS_ENABLED(CONFIG_NRF_RPC_SHMEM_PRIMARY) ?
					   "IPM_1" : "IPM_0");
	if (!ipm_tx_handle) {
		LOG_ERR("Could not get TX IPM device handle");
		return -NRF_ENODEV;
	}

	ipm_rx_handle = device_get_binding(IS_ENABLED(CONFIG_NRF_RPC_SHMEM_PRIMARY) ?
					   "IPM_0" : "IPM_1");
	if (!ipm_rx_handle) {
		LOG_ERR("Could not get RX IPM device handle");
		return -NRF_ENODEV;
	}

	ipm_register_callback(ipm_rx_handle, ipm_callback, NULL);

	return 0;
}

#else 

static int shmem_init() {
	return 0;
}

#endif /* CONFIG_NRF_RPC_TR_SHMEM */
