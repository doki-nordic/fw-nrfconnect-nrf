
// https://en.wikipedia.org/wiki/Async/await

/**
 * @brief Macro to start an async function.
 *
 * It uses a static data to keep its state, so it can be used only for single-instance coroutines.
 */
#define STATIC_COROUTINE_START \
	static void* _resume_point = &&_resume_label_start; \
	goto *_resume_point; \
    _empty_resume_point: \
    return; \
	_resume_label_start: \
	do { } while (0);

/**
 * @brief Macro that awaits in coroutine.
 *
 * It sets the resume point in place where is was used and checks the condition.
 * If the condition is false, it suspends the coroutine.
 * When coroutine is triggered again, and the condition is true,
 * it resumes the continues and continues after the AWAIT.
 */
#define AWAIT(condition) _AWAIT_IMPL_1(condition, __LINE__, __COUNTER__)
#define _AWAIT_IMPL_1(condition, line, counter) _AWAIT_IMPL_2(condition, line, counter)
#define _AWAIT_IMPL_2(condition, line, counter) do { \
        _resume_point = &&_resume_label_##line##_##counter; \
        _resume_label_##line##_##counter: \
        if (!(condition)) { COROUTINE_SUSPEND; } \
    } while (0)

/**
 * @brief Macro to suspend the coroutine.
 * 
 * You can suspend the coroutine unconditionally with this macro.
 * The coroutine will be resumed at the latest AWAIT() point.
 *
 * This is useful when you want to do some more complicated logic for coroutine resume than just checking a condition.
 * You can do AWAIT(true). It will always resume immediately and after each coroutine trigger.
 * If you still want to go back into the suspended state, you can call COROUTINE_SUSPEND to suspend it again.
 */
#define COROUTINE_SUSPEND return

/**
 * @brief Macro to return from the coroutine.
 *
 * This will set the resume point to start of the coroutine (where STATIC_COROUTINE_START is),
 * so the next coroutine trigger will start it from the beginning.
 */
#define COROUTINE_RETURN do { _resume_point = &&_resume_label_start; return; } while (0)


#define COROUTINE_IGNORE_TRIGGERS do { _resume_point = &&_empty_resume_point; } while (0)

/* --------------------------------------------------- */

enum esb_trigger_reason {
    ESB_TRIGGER_REASON_START_TX,     // User called esb_start_tx()
    ESB_TRIGGER_REASON_RADIO_IRQ,    // Radio IRQ
    ESB_TRIGGER_REASON_TIMER_IRQ,    // Timer IRQ
};

/* --------------------------------------------------- */


void esb_ptx_coroutine(enum esb_trigger_reason reason)
{
    static bool ack_received;
    static int retransmission_counter;
    STATIC_COROUTINE_START;

    // Loop over all send requests
    while (1) {

        // Wait for send request
        AWAIT(fifo_non_empty());
        // Initialize radio
        radio_init();

        // Setup packet and shorts:
        //   - fast switching: READY -> START, END? -> RXEN
        //   - normal:         READY -> START, END? -> DISABLE, DISABLED -> RXEN
        //   - no ACK:         READY -> START, END? -> DISABLE
        //   - others? never disable TX?

        setup_and_send_packet();

        if (!noack) {

            // loop over retransmissions
            for (retransmission_counter = 0; retransmission_counter <= max_retransmits; retransmission_counter++) {

                // Setup timer to wait for ACK
                // PPI: RADIO_END -> TIMER_START
                //      TIMER_COMPARE0 -> RADIO_DISABLE
                //      TIMER_COMPARE1 -> RADIO_TXEN (if not last retransmission) - WHAT IF DIFFERENCE BETWEEN THOSE IS TOO SMALL?
                setup_timer_for_ack_timeout();
                ack_received = true;

                // Since packet pointer register is double buffered,
                // we can setup receiving as soon as we know data sending was started.
                AWAIT(
                    reason == ESB_TRIGGER_REASON_RADIO_IRQ &&
                    nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_PAYLOAD));
                nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_PAYLOAD);

                // Setup receiving buffer as soon as possible
                setup_receiving();

                // Wait until RX is ready
                AWAIT(
                    reason == ESB_TRIGGER_REASON_RADIO_IRQ &&
                    nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_READY));
                nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_READY);

                // Change shorts related to END event
                if (fast_switching && fifo_non_empty) {
                    // END -> TXEN
                } else {
                    // END -> DISABLE
                }

                // The `AWAIT(true)` will resume immediately. Later, you can suspend last AWAIT with COROUTINE_SUSPEND.
                // TODO: maybe create a special macro for this, e.g. COROUTINE_RESUME_POINT?
                AWAIT(true);
                if (fast_switching && reason == ESB_TRIGGER_REASON_START_TX && !short_enabled(END_TXEN)) {
                    // User requested next transmission while we were waiting for ACK, so turn on short for fast switching
                    // turn off: END -> DISABLE
                    // turn on: END -> TXEN
                    // and continue waiting for ACK
                    COROUTINE_SUSPEND;
                } else if (reason == ESB_TRIGGER_REASON_TIMER_IRQ) {
                    // Timeout waiting for ACK
                    ack_received = false;
                    // Continue with retransmission, but only if we have not reached max retransmissions
                    if (retransmission_counter == max_retransmits) {
                        break;
                    }
                } else if (reason == ESB_TRIGGER_REASON_RADIO_IRQ && nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_END)) {
                    nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_END);
                    ack_received = validate_ack();
                    break; // ACK received, so exit the retransmission loop
                } else {
                    COROUTINE_SUSPEND;
                }

                // Retransmission needed
                // ...
                // ...
            }

            // We are going to call a user callback and user can call esb_start_tx() from it, so the
            // coroutine will be triggered while another trigger is executing. We don't want that, so we set
            // the resume point to ignore all the triggers until we exit the callback.
            COROUTINE_IGNORE_TRIGGERS;
            if (ack_received) {
                user_callback_tx_success();
                if (data_in_ack) {
                    user_callback_rx_received();
                }
            } else {
                user_callback_tx_failed();
            }
        } else {
            if (never_disable_tx) {
                // do nothing, we are waiting for a next packet in TXIDLE state
            } else {
                // Wait for radio to be disabled completely
                AWAIT(
                    reason == ESB_TRIGGER_REASON_RADIO_IRQ &&
                    nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_DISABLED));
            }
        }
    }
    STATIC_COROUTINE_END;
}

void RADIO_IRQHandler(void) {
    esb_ptx_coroutine(ESB_TRIGGER_REASON_RADIO_IRQ);
}

void TIMER_IRQHandler(void) {
    esb_ptx_coroutine(ESB_TRIGGER_REASON_TIMER_IRQ);
}

int esb_start_tx(void) {
    // ...
	unsigned int key = irq_lock();
    esb_ptx_coroutine(ESB_TRIGGER_REASON_START_TX);
	irq_unlock(key);
}


// ----------------------------------------------------------------------------------------
//                      POSSIBLE EXTENSIONS (if needed)
// ----------------------------------------------------------------------------------------


// ---------------------------------- EXCEPTIONS EXAMPLE

// Exceptions can be used to handle some conditions that can happen anywhere in the coroutine,
// e.g. fatal error, user called uninitialize function, etc.

#define COROUTINE_EXCEPTION_HANDLER(condition, label) \
    do { \
        if (condition) { \
            goto label; \
        } \
    } while (0)


void exceptions_example(enum esb_trigger_reason event)
{
    static bool ack_received;
    COROUTINE_EXCEPTION_HANDLER(event == ESB_TRIGGER_REASON_FATAL, fatal_error);
    COROUTINE_EXCEPTION_HANDLER(event == ESB_TRIGGER_REASON_UNINIT, uninitialized);
    STATIC_COROUTINE_START;

    radio_init();

    while (1) {
        // The AWAIT may end up with exception, so code must be prepared that AWAIT will jump
        // immediately to the exception handler.
        AWAIT(something());
    }

fatal_error:
    LOG_ERR("Fatal error occurred!");

uninitialized:
    disable_radio();
    COROUTINE_RETURN;
}


// ---------------------------------- CALLING ASYNC FUNCTIONS FROM COROUTINE

enum coroutine_status {
    COROUTINE_IN_PROGRESS = 0,
    COROUTINE_DONE = 1,
};

// To call subcoroutines, we need to modify the macros:
// - AWAIT(...) returns COROUTINE_IN_PROGRESS,
// - STATIC_COROUTINE_START returns COROUTINE_IN_PROGRESS in _empty_resume_point,
// - COROUTINE_SUSPEND returns COROUTINE_IN_PROGRESS,
// - COROUTINE_RETURN returns COROUTINE_DONE.

// Macro to tell that it is an async function.
// The coroutine_status type should be internal to the macros, so user does not need to know about it.
#define ASYNC enum coroutine_status


ASYNC awaitable_function(int event, int param)
{
    STATIC_COROUTINE_START;

    // Do something
    while (some_condition(param)) {
        AWAIT(do_something(param));
    }

    COROUTINE_RETURN;
}

ASYNC coroutine_main_function(int event)
{
    static int i;
    STATIC_COROUTINE_START;

    // Do something
    for (i = 0; i < 10; i++) {
        AWAIT(awaitable_function(event, i));
    }

    COROUTINE_RETURN;
}
