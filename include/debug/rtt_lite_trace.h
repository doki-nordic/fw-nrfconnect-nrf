/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef _RTT_LIGHT_TRACE_H
#define _RTT_LIGHT_TRACE_H
#include <kernel.h>
#include <kernel_structs.h>
#include <init.h>

/*
TODO: Use SysTick as clock source.
- Read directly from SYST_CVR: https://developer.arm.com/documentation/dui0552/a/cortex-m3-peripherals/system-timer--systick
- Send SYST_RVR value at startup (and maybe periodically)
- On host side, use SysTick interrupt to tell when the counter was reloaded. Be careful, because interrupt may be delayed sometimes,
  so for safety use formula: time_hi = max(time_hi, time_hi_from_previous_interrupt + period)
- Warning on host side if the interrupts tracking is disabled and we are using SysTick
- Optionally, allow user to implement the time function (don't use Nordic HW timers)
*/

/* Definitions used internally in this header file. */
#define _RTT_LITE_TRACE_EV_MARK_START 0x19000000
#define _RTT_LITE_TRACE_EV_MARK 0x1A000000
#define _RTT_LITE_TRACE_EV_MARK_STOP 0x1B000000
#define _RTT_LITE_TRACE_EV_NUMBER_FIRST_USER 0x1D000000
#define _RTT_LITE_TRACE_EV_NUMBER_FIRST_RESERVED 0x7C000000

/** @brief Defines new user event that can be traced.
 * 
 * Event id is defined as unnamed enum containing one item with provided event
 * name and value. Static assert checks if the number is in correct range.
 * 
 * @param name Name of the event.
 * @param num  User event number. The numbers starts at zero.
 */
#define RTT_LITE_TRACE_USER_EVENT(name, num) \
	enum { \
		name = ((uint32_t)(num) << 24) + \
		_RTT_LITE_TRACE_EV_NUMBER_FIRST_USER \
	}; \
	BUILD_ASSERT((uint32_t)(num) < \
		((uint32_t)(_RTT_LITE_TRACE_EV_NUMBER_FIRST_RESERVED - \
		_RTT_LITE_TRACE_EV_NUMBER_FIRST_USER) >> 24), \
		"User event number is invalid!");

#define RTT_LITE_TRACE_LEVEL_LOG 0
#define RTT_LITE_TRACE_LEVEL_WARN 1
#define RTT_LITE_TRACE_LEVEL_ERR 2

#define RTT_LITE_TRACE_FORMAT_INIT(_level, format_string) { \
	.text = (format_string), .id = 0, .level = (_level), .args = { 0 } }

#define RTT_LITE_TRACE_PRINTF(level, format_string, ...) \
	do { \
		static struct rtt_lite_trace_format _lite_trace_fmt = \
			RTT_LITE_TRACE_FORMAT_INIT(level, format_string); \
		rtt_lite_trace_printf(&_lite_trace_fmt, ##__VA_ARGS__); \
	} while (0)

#define RTT_LITE_TRACE_LOGF(format, ...) \
	RTT_LITE_TRACE_PRINTF(RTT_LITE_TRACE_LEVEL_LOG, format, ##__VA_ARGS__)
#define RTT_LITE_TRACE_WARNF(format, ...) \
	RTT_LITE_TRACE_PRINTF(RTT_LITE_TRACE_LEVEL_WARN, format, ##__VA_ARGS__)
#define RTT_LITE_TRACE_ERRF(format, ...) \
	RTT_LITE_TRACE_PRINTF(RTT_LITE_TRACE_LEVEL_ERR, format, ##__VA_ARGS__)
#define RTT_LITE_TRACE_LOG(text) \
	rtt_lite_trace_print(RTT_LITE_TRACE_LEVEL_LOG, (text))
#define RTT_LITE_TRACE_WARN(text) \
	rtt_lite_trace_print(RTT_LITE_TRACE_LEVEL_WARN, (text))
#define RTT_LITE_TRACE_ERR(text) \
	rtt_lite_trace_print(RTT_LITE_TRACE_LEVEL_ERR, (text))

struct rtt_lite_trace_format {
	const char *text;
	uint32_t id;
	uint8_t level;
	uint8_t args[CONFIG_RTT_LITE_TRACE_PRINTF_MAX_ARGS + 1];
};

uint32_t rtt_lite_trace_time(void);

void rtt_lite_trace_print(uint32_t level, const char *text);
void rtt_lite_trace_printf(struct rtt_lite_trace_format *format, ...);

static inline void rtt_lite_trace_mark_start(uint32_t mark_id);
static inline void rtt_lite_trace_mark(uint32_t mark_id);
static inline void rtt_lite_trace_mark_stop(uint32_t mark_id);

static inline void rtt_lite_trace_call(uint32_t event);
static inline void rtt_lite_trace_call_1(uint32_t event, uint32_t arg1);
void rtt_lite_trace_call_v(uint32_t event, uint32_t num_args, uint32_t arg1, ...);
void rtt_lite_trace_name(uint32_t resource_id, const char *name);

void sys_trace_thread_switched_in(void);
void sys_trace_thread_switched_out(void);
void sys_trace_isr_enter(void);
void sys_trace_isr_exit(void);
void sys_trace_isr_exit_to_scheduler(void);
void sys_trace_idle(void);

void sys_trace_thread_priority_set(k_tid_t thread);
void sys_trace_thread_create(k_tid_t thread);
void sys_trace_thread_suspend(k_tid_t thread);
void sys_trace_thread_resume(k_tid_t thread);
void sys_trace_thread_ready(k_tid_t thread);
void sys_trace_thread_pend(k_tid_t thread);
#ifdef CONFIG_RTT_LITE_TRACE_THREAD_INFO
void sys_trace_thread_name_set(k_tid_t thread);
#else
#define sys_trace_thread_name_set(thread)
#endif
#ifdef CONFIG_RTT_LITE_TRACE_SYNCHRO
void sys_trace_void(uint32_t id);
void sys_trace_end_call(uint32_t id);
#else
#define sys_trace_void(id)
#define sys_trace_end_call(id)
#endif

/* Trace macros that are ignored */
#define sys_trace_thread_abort(thread)

/* Trace macros that are actually never called by the Zephyr kernel */
#define sys_trace_thread_info(thread)

static inline void rtt_lite_trace_mark_start(uint32_t mark_id)
{
	void rtt_lite_trace_event(uint32_t event, uint32_t param);
	rtt_lite_trace_event(_RTT_LITE_TRACE_EV_MARK_START, mark_id);
}

static inline void rtt_lite_trace_mark(uint32_t mark_id)
{
	void rtt_lite_trace_event(uint32_t event, uint32_t param);
	rtt_lite_trace_event(_RTT_LITE_TRACE_EV_MARK, mark_id);
}

static inline void rtt_lite_trace_mark_stop(uint32_t mark_id)
{
	void rtt_lite_trace_event(uint32_t event, uint32_t param);
	rtt_lite_trace_event(_RTT_LITE_TRACE_EV_MARK_STOP, mark_id);
}

static inline void rtt_lite_trace_call(uint32_t event)
{
	void rtt_lite_trace_event(uint32_t event, uint32_t param);
	rtt_lite_trace_event(event, 0);
}

static inline void rtt_lite_trace_call_1(uint32_t event, uint32_t arg1)
{
	void rtt_lite_trace_event(uint32_t event, uint32_t param);
	rtt_lite_trace_event(event, arg1);
}

#endif /* _RTT_LIGHT_TRACE_H */
