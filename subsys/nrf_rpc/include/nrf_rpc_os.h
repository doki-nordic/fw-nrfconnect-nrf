/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRF_RPC_OS_H_
#define NRF_RPC_OS_H_

#include <zephyr.h>

/**
 * @defgroup nrf_rpc_os_zephyr nRF PRC OS abstraction for Zephyr.
 * @{
 * @brief nRF PRC OS abstraction for Zephyr.
 *
 * API is compatible with nrf_rpc_os API. For API documentation
 * @see nrf_rpc_os_tmpl.h
 */

#ifdef __cplusplus
extern "C" {
#endif

struct nrf_rpc_os_event {
	struct k_sem sem;
};

struct nrf_rpc_os_msg {
	struct k_sem sem;
	const uint8_t *data;
	size_t len;
};

typedef void (*nrf_rpc_os_work_t)(const uint8_t *data, size_t len);

int nrf_rpc_os_init(nrf_rpc_os_work_t callback);

void nrf_rpc_os_thread_pool_send(const uint8_t *data, size_t len);

static inline int nrf_rpc_os_event_init(struct nrf_rpc_os_event *event)
{
	return k_sem_init(&event->sem, 0, 1);
}

static inline void nrf_rpc_os_event_set(struct nrf_rpc_os_event *event)
{
	k_sem_give(&event->sem);
}

static inline void nrf_rpc_os_event_wait(struct nrf_rpc_os_event *event)
{
	k_sem_take(&event->sem, K_FOREVER);
}

static inline int nrf_rpc_os_msg_init(struct nrf_rpc_os_msg *msg)
{
	return k_sem_init(&msg->sem, 0, 1);
}

void nrf_rpc_os_msg_set(struct nrf_rpc_os_msg *msg, const uint8_t *data,
			size_t len);

void nrf_rpc_os_msg_get(struct nrf_rpc_os_msg *msg, const uint8_t **data,
			size_t *len);

static inline void *nrf_rpc_os_tls_get(void)
{
	return k_thread_custom_data_get();
}

static inline void nrf_rpc_os_tls_set(void *data)
{
	k_thread_custom_data_set(data);
}

uint32_t nrf_rpc_os_ctx_pool_reserve(void);
void nrf_rpc_os_ctx_pool_release(uint32_t number);

void nrf_rpc_os_remote_count(int count);

static inline void nrf_rpc_os_remote_reserve(void)
{
	extern struct k_sem _nrf_rpc_os_remote_counter;

	k_sem_take(&_nrf_rpc_os_remote_counter, K_FOREVER);
}

static inline void nrf_rpc_os_remote_release(void)
{
	extern struct k_sem _nrf_rpc_os_remote_counter;

	k_sem_give(&_nrf_rpc_os_remote_counter);
}

#ifdef CONFIG_NRF_RPC_TR_SHMEM

#define NRF_RPC_OS_SHMEM_PTR_CONST 0

extern void *nrf_rpc_os_out_shmem_ptr;
extern void *nrf_rpc_os_in_shmem_ptr;

#define NRF_RPC_OS_MEMORY_BARIER() __DSB()

void nrf_rpc_os_signal(void);
void nrf_rpc_os_signal_handler(void (*handler)(void));

typedef atomic_t nrf_rpc_os_atomic_t;
#define nrf_rpc_os_atomic_or atomic_or
#define nrf_rpc_os_atomic_and atomic_and
#define nrf_rpc_os_atomic_get atomic_get

typedef struct k_mutex nrf_rpc_os_mutex_t;
#define nrf_rpc_os_mutex_init k_mutex_init
#define nrf_rpc_os_unlock k_mutex_unlock
static inline void nrf_rpc_os_lock(nrf_rpc_os_mutex_t *mutex) {
	k_mutex_lock(mutex, K_FOREVER);
}

typedef struct k_sem nrf_rpc_os_sem_t;
#define nrf_rpc_os_give k_sem_give
static inline void nrf_rpc_os_sem_init(nrf_rpc_os_sem_t *sem) {
	k_sem_init(sem, 0, 1);
}
static inline void nrf_rpc_os_take(nrf_rpc_os_sem_t *sem) {
	k_sem_take(sem, K_FOREVER);
}

#define nrf_rpc_os_yield k_yield
#define nrf_rpc_os_fatal k_oops
#define nrf_rpc_os_clz64 __builtin_clzll
#define nrf_rpc_os_clz32 __builtin_clz

#endif /* CONFIG_NRF_RPC_TR_SHMEM */

#ifdef __cplusplus
}
#endif

/**
 *@}
 */

#endif /* NRF_RPC_OS_H_ */
