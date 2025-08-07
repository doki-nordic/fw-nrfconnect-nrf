/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DTM_UART_WAIT_H_
#define DTM_UART_WAIT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define DTM_UART DT_CHOSEN(ncs_dtm_uart)

/** @brief Initialize wait function.
 *
 * @return 0 in case of success or negative value in case of error.
 */
int dtm_uart_wait_init(void);

/** @brief Wait for UART poll cycle.
 *
 * Wait for half of the UART period used by the DTM.
 */
void dtm_uart_wait(void);

extern const char* volatile _my_error_text;
extern atomic_t _my_log_counter;

#define MY_LOG(text, ...) do { \
	int cnt = atomic_inc(&_my_log_counter); \
	const char* _err_text = _my_error_text; \
	if (_err_text) \
		LOG_ERR("%d: " text " (ACTIVE ERROR: %s)", cnt, ##__VA_ARGS__, _err_text); \
	else \
		LOG_ERR("%d: " text, cnt, ##__VA_ARGS__); \
} while (0)

#define ENTER_FUNC() \
	static atomic_t _enter_count; \
	int _old_count = atomic_inc(&_enter_count); \
	MY_LOG("ENTER %s: %d", __FUNCTION__, _old_count); \
	if (_old_count > 0) { _my_error_text = "Multiple enters"; }

#define EXIT_FUNC() \
	int _old_dec_count = atomic_dec(&_enter_count); \
	MY_LOG("EXIT  %s: %d (current: %d)", __FUNCTION__, _old_count, _old_dec_count - 1);

#ifdef __cplusplus
}
#endif

#endif /* DTM_UART_WAIT_H_ */
