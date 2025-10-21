

/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
#include <nrf.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/pm/device_runtime.h>
#include <dk_buttons_and_leds.h>
#if defined(CONFIG_CLOCK_CONTROL_NRF2)
#include <hal/nrf_lrcconf.h>
#endif
#include <nrf_erratas.h>
#if NRF54L_ERRATA_20_PRESENT
#include <hal/nrf_power.h>
#endif /* NRF54L_ERRATA_20_PRESENT */
#if defined(NRF54LM20A_ENGA_XXAA)
#include <hal/nrf_clock.h>
#endif /* defined(NRF54LM20A_ENGA_XXAA) */
#include "nrfx_gpiote.h"


#define TEST_CHANNEL 0
#define TEST_PIN NRF_GPIO_PIN_MAP(1, 4)
static nrfx_gpiote_t const gpiote = NRFX_GPIOTE_INSTANCE(0);

int main() {
    int i = 0;
    int err = nrfx_gpiote_init(&gpiote, NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY);
    printk("nrfx_gpiote_init: %d\n", err - NRFX_ERROR_BASE_NUM);

    nrfx_gpiote_output_config_t out_cfg = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;
    nrfx_gpiote_task_config_t task_cfg = {
        .task_ch = TEST_CHANNEL,
        .polarity = NRF_GPIOTE_POLARITY_TOGGLE,
        .init_val = NRF_GPIOTE_INITIAL_VALUE_HIGH
    };

    err = nrfx_gpiote_output_configure(&gpiote, TEST_PIN, &out_cfg, &task_cfg);
    printk("nrfx_gpiote_output_configure: %d\n", err - NRFX_ERROR_BASE_NUM);

    nrfx_gpiote_out_task_enable(&gpiote, TEST_PIN);

    uint32_t task_addr = nrfx_gpiote_out_task_address_get(&gpiote, TEST_PIN);
    printk("nrfx_gpiote_out_task_address_get: 0x%08X\n", task_addr);

    while (1) {
		k_sleep(K_MSEC(1000));
        printk("Hello, World %d!\n", i);
        i++;
        *(volatile uint32_t*)task_addr = 1;
    }
}

#if 0

//#include "nrf_test_runner.h"
#include "nrfx_gpiote.h"

#define TEST_MODE AUTOMATIC_TEST_MODE
#define TEST_NAME "GPIOTE Test"

static volatile uint32_t m_handler_called;

#define NRFX_GPIOTE_IRQN(_num) NRFX_CONCAT(GPIOTE, _num, _IRQn)
#define NRFX_GPIOTE_IRQ_HANDLER(_num) NRFX_CONCAT(nrfx_gpiote_, _num, _irq_handler)

/* TODO: Skip specific tests on PPR until NRFX-3213 is fixed. */
#if defined(NRF_PPR)
#define PPR_SKIP_TESTS 1
#endif

static const nrfx_gpiote_pin_t test_pins[] = {
    NRFX_GPIOTE_TEST_OUT_0_PIN,
    NRFX_GPIOTE_TEST_OUT_1_PIN,
    NRFX_GPIOTE_TEST_OUT_2_PIN,
    NRFX_GPIOTE_TEST_IN_0_PIN,
    NRFX_GPIOTE_TEST_IN_1_PIN,
    NRFX_GPIOTE_TEST_IN_2_PIN
};

static nrfx_gpiote_t const gpiote = NRFX_GPIOTE_INSTANCE(NRFX_GPIOTE_GPIOTE_INSTANCE_NUM);

#ifdef CONFIG_TEST_GPIOTE_0

#ifdef CONFIG_TEST_GPIOTE_0_OUT_PIN
#define GPIOTE_FIXED_OUT_CH 0
#else
#define GPIOTE_FIXED_IN_CH 1
#endif

static nrfx_gpiote_t const gpiote0 = NRFX_GPIOTE_INSTANCE(0); // GPIOTE130
#define GPIOTE0_IRQn GPIOTE_0_IRQn
#endif


static nrfx_gpiote_t const * p_gpiote0 =
    NRFX_COND_CODE_1(CONFIG_TEST_GPIOTE_0, (&gpiote0), (&gpiote));

static uint32_t channels_number;

void setUp(void)
{
    NRFX_CUSTOM_IRQ_CONNECT(NRFX_GPIOTE_IRQN(NRFX_GPIOTE_GPIOTE_INSTANCE_NUM),
                            NRFX_GPIOTE_IRQ_HANDLER(NRFX_GPIOTE_GPIOTE_INSTANCE_NUM),
                            APP_IRQ_PRIORITY_LOW);
#ifdef GPIOTE0_IRQn
    NRFX_CUSTOM_IRQ_CONNECT(GPIOTE0_IRQn, nrfx_gpiote_0_irq_handler, APP_IRQ_PRIORITY_LOW);
#endif

    channels_number = nrfx_gpiote_channels_number_get(&gpiote);
#if defined(ISA_RISCV)
    nrf_vpr_csr_machine_interrupts_enable();
#endif

#if defined(HALTIUM_XXAA) || defined(LUMOS_XXAA)
    /* GPIO/GPIOTE state is retained on haltium so it must be reset to the
     * known state.
     */
    nrf_gpiote_int_disable(gpiote.p_reg, 0xFFFFFFFF);

    for (uint32_t i = 0; i < channels_number; i++)
    {
        gpiote.p_reg->CONFIG[i] = GPIOTE_CONFIG_ResetValue;
        gpiote.p_reg->EVENTS_IN[i] = GPIOTE_EVENTS_IN_ResetValue;
    }

    for (size_t i = 0; i < NRFX_ARRAY_SIZE(test_pins); i++)
    {
        uint32_t pin_number = test_pins[i];
        NRF_GPIO_Type * reg = nrf_gpio_pin_port_decode(&pin_number);

        reg->LATCH = GPIO_LATCH_ResetValue;
        reg->DIR = GPIO_DIR_ResetValue;
        reg->OUT = GPIO_OUT_ResetValue;
        reg->PIN_CNF[pin_number] = GPIO_PIN_CNF_ResetValue;
    }

#endif
    m_handler_called = 0;

    /* Ignore return value in case the driver was already initialized. */
    (void)nrfx_gpiote_init(&gpiote, NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY);

    /* Ignore return value in case the driver was already initialized. */
    if (&gpiote != p_gpiote0)
    {
        (void)nrfx_gpiote_init(p_gpiote0, NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY);
    }
}

void tearDown(void)
{
    if (nrfx_gpiote_init_check(&gpiote))
    {
        nrfx_gpiote_uninit(&gpiote);
    }
    //reset pin status, it's not fully reseted during sw reset
    for (size_t i = 0; i < NRFX_ARRAY_SIZE(test_pins); i++)
    {
        nrf_gpio_pin_clear(test_pins[i]);
        nrf_gpio_cfg_default(test_pins[i]);
    }
}

static void event_handler1(nrfx_gpiote_pin_t     pin,
                           nrfx_gpiote_trigger_t trigger,
                           void *                p_context)
{
    (void)p_context;
    TEST_ASSERT_EQUAL_UINT16(NRFX_GPIOTE_TEST_OUT_0_PIN, pin);
    TEST_ASSERT_EQUAL(NRFX_GPIOTE_TRIGGER_HITOLO, trigger);
    m_handler_called++;
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_simple_port_event_watcher)
{
    nrfx_err_t err;

#if defined(GPIOTE_FIXED_IN_CH) || defined(GPIOTE_FIXED_OUT_CH)
    // Port event not supported by GPIOTE0 on cpurad
    return;
#endif
    // Output initialy set HIGH.
    nrfx_gpiote_output_config_t out_config = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;
    out_config.input_connect = NRF_GPIO_PIN_INPUT_CONNECT;

    err = nrfx_gpiote_output_configure(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN, &out_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    nrfx_gpiote_out_set(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN);

    nrfx_gpiote_trigger_config_t trigger_cfg =
    {
        .trigger = NRFX_GPIOTE_TRIGGER_HITOLO
    };
    nrfx_gpiote_handler_config_t handler_cfg =
    {
        .handler = event_handler1
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = NULL,
        .p_trigger_config = &trigger_cfg,
        .p_handler_config = &handler_cfg,
    };

    err = nrfx_gpiote_input_configure(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN, true);
    // Make the first HITOLO transition.
    nrfx_gpiote_out_clear(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN);

    // Apparently some (but quite big) delay is needed for the interrupt to
    // be generated after pin state change is applied.
    // It would be good to investigate and explain why.
    nrf_test_delay_us(1);
    nrfx_gpiote_out_set(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN);
    // Make the second HITOLO transition.
    nrfx_gpiote_out_clear(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_test_delay_us(1);
    // Expect two handler calls with HITOLO action detected on NRFX_GPIOTE_TEST_OUT_0_PIN.
    TEST_ASSERT_EQUAL_INT(2, m_handler_called);

    nrfx_gpiote_trigger_disable(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrfx_gpiote_pin_uninit(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN);
}

static void event_handler2(nrfx_gpiote_pin_t     pin,
                           nrfx_gpiote_trigger_t trigger,
                           void *                p_context)
{
    (void)p_context;
    TEST_ASSERT_EQUAL_UINT16(NRFX_GPIOTE_TEST_IN_0_PIN,pin);
    TEST_ASSERT_EQUAL(NRFX_GPIOTE_TRIGGER_LOTOHI, trigger);
    m_handler_called++;
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_simple_pin_event_generated_when_connected_output_is_changed)
{
    uint8_t ch;
    nrfx_err_t err;

#ifdef GPIOTE_FIXED_OUT_CH
    return;
#endif

#ifdef GPIOTE_FIXED_IN_CH
    ch = GPIOTE_FIXED_IN_CH;
#else
    err = nrfx_gpiote_channel_alloc(&gpiote, &ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
#endif

    nrfx_gpiote_output_config_t out_cfg = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;

    nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_trigger_config_t trigger_cfg =
    {
        .trigger = NRFX_GPIOTE_TRIGGER_LOTOHI,
        .p_in_channel = &ch
    };
    nrfx_gpiote_handler_config_t handler_cfg =
    {
        .handler = event_handler2
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_cfg,
        .p_handler_config = &handler_cfg,
    };

    err = nrfx_gpiote_output_configure(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN, &out_cfg, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    err = nrfx_gpiote_input_configure(p_gpiote0, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(p_gpiote0, NRFX_GPIOTE_TEST_IN_0_PIN, true);

    nrfx_gpiote_out_set(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_test_delay_us(4);
    TEST_ASSERT_EQUAL_INT(1, m_handler_called);
    nrfx_gpiote_out_clear(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrfx_gpiote_trigger_disable(p_gpiote0, NRFX_GPIOTE_TEST_IN_0_PIN);

#ifdef GPIOTE_FIXED_IN_CH
    // GPIOTE0 on radio does not support PORT
    return;
#endif

    //same but with low accuracy pin
    trigger_cfg.p_in_channel = NULL;
    err = nrfx_gpiote_input_configure(p_gpiote0, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_channel_free(&gpiote, ch);

    nrfx_gpiote_trigger_enable(p_gpiote0, NRFX_GPIOTE_TEST_IN_0_PIN, true);

    nrfx_gpiote_out_set(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_test_delay_us(4);
    TEST_ASSERT_EQUAL_INT(2, m_handler_called);

    nrfx_gpiote_trigger_disable(p_gpiote0, NRFX_GPIOTE_TEST_IN_0_PIN);
}

static void event_handler3(nrfx_gpiote_pin_t     pin,
                           nrfx_gpiote_trigger_t trigger,
                           void *                p_context)
{
    (void)p_context;
    TEST_ASSERT_EQUAL_UINT16(NRFX_GPIOTE_TEST_IN_0_PIN,pin);
    TEST_ASSERT_EQUAL(NRFX_GPIOTE_TRIGGER_TOGGLE, trigger);
    m_handler_called++;
}

static void _test_case_simple_pin_event_generated_when_connected_output_is_toggled(void)
{
    nrfx_err_t err;
    nrfx_gpiote_output_config_t out_config = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;
    uint8_t ch;
#ifdef GPIOTE_FIXED_OUT_CH
    return;
#endif

#ifdef GPIOTE_FIXED_IN_CH
    ch = GPIOTE_FIXED_IN_CH;
#else
    err = nrfx_gpiote_channel_alloc(&gpiote, &ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
#endif

    err = nrfx_gpiote_output_configure(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN, &out_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_trigger_config_t trigger_cfg =
    {
        .trigger = NRFX_GPIOTE_TRIGGER_TOGGLE,
        .p_in_channel = &ch
    };
    nrfx_gpiote_handler_config_t handler_cfg =
    {
        .handler = event_handler3
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_cfg,
        .p_handler_config = &handler_cfg,
    };

    err = nrfx_gpiote_input_configure(p_gpiote0, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(p_gpiote0, NRFX_GPIOTE_TEST_IN_0_PIN, true);

    nrfx_gpiote_out_toggle(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_test_delay_us(4);
    nrfx_gpiote_out_toggle(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_test_delay_us(4);
    //Workaround for a GCC problem:
    static volatile uint32_t dummy[2];
    if (dummy[m_handler_called]) {}

    TEST_ASSERT_EQUAL_INT(2, m_handler_called);

    nrfx_gpiote_trigger_disable(p_gpiote0, NRFX_GPIOTE_TEST_IN_0_PIN);

    if (NRFX_IS_ENABLED(GPIOTE_FIXED_IN_CH))
    {
        nrfx_gpiote_channel_free(&gpiote, ch);
    }
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_simple_pin_event_generated_when_connected_output_is_toggled)
{
    _test_case_simple_pin_event_generated_when_connected_output_is_toggled();
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_ppi_task_can_control_out_task)
{
    nrfx_err_t err;
    uint8_t ch_task;
    uint8_t ch_evt;
    const nrfx_gpiote_t * out_gpiote = &gpiote;
    const nrfx_gpiote_t * in_gpiote = &gpiote;

#ifdef GPIOTE_FIXED_OUT_CH
    ch_task = GPIOTE_FIXED_OUT_CH;
    out_gpiote = p_gpiote0;
#else
    err = nrfx_gpiote_channel_alloc(&gpiote, &ch_task);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
#endif

#ifdef GPIOTE_FIXED_IN_CH
    ch_evt = GPIOTE_FIXED_IN_CH;
    in_gpiote = p_gpiote0;
#else
    err = nrfx_gpiote_channel_alloc(&gpiote, &ch_evt);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
#endif

    nrfx_gpiote_output_config_t out_cfg = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;
    nrfx_gpiote_task_config_t task_cfg = {
        .task_ch = ch_task,
        .polarity = NRF_GPIOTE_POLARITY_TOGGLE,
        .init_val = NRF_GPIOTE_INITIAL_VALUE_HIGH
    };

    err = nrfx_gpiote_output_configure(out_gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN,
                                       &out_cfg, &task_cfg);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_out_task_enable(out_gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN);

    //get ppi address of task releated to initialized pin
    uint32_t task_addr = nrfx_gpiote_out_task_address_get(out_gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN);

    nrf_gpio_pin_pull_t pull_cfg = NRF_GPIO_PIN_PULLUP;
    nrfx_gpiote_trigger_config_t trigger_cfg =
    {
        .trigger = NRFX_GPIOTE_TRIGGER_TOGGLE,
        .p_in_channel = &ch_evt
    };
    nrfx_gpiote_handler_config_t handler_cfg =
    {
        .handler = event_handler3
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_cfg,
        .p_handler_config = &handler_cfg,
    };

    err = nrfx_gpiote_input_configure(in_gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    nrfx_gpiote_trigger_enable(in_gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, true);

    //toggle twice output pin using TASKs
    *(volatile uint32_t*)task_addr = 1;
    //minimal delay is needed, otherwise probably pin is toggled twice before entering irq
    nrf_test_delay_us(2);
    *(volatile uint32_t*)task_addr = 1;
    nrf_test_delay_us(4);
    TEST_ASSERT_EQUAL_INT(2, m_handler_called);

    nrfx_gpiote_out_task_disable(out_gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrfx_gpiote_trigger_disable(in_gpiote, NRFX_GPIOTE_TEST_IN_0_PIN);
    nrfx_gpiote_channel_free(&gpiote, ch_task);
    nrfx_gpiote_channel_free(&gpiote, ch_evt);
}

void check_gpio_initial_state(void)
{
    for (size_t i = 0; i < NRFX_ARRAY_SIZE(test_pins); i++)
    {
        TEST_ASSERT_EQUAL_UINT32(NRF_GPIO_PIN_INPUT_DISCONNECT,
                                 nrf_gpio_pin_input_get(test_pins[i]));
    }
}

void check_gpiote_initial_state(void)
{
    uint32_t i;
    for (i = 0; i < channels_number; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(0x00000000, gpiote.p_reg->CONFIG[i] & GPIOTE_CONFIG_MODE_Msk);
        TEST_ASSERT_EQUAL_UINT32(0x00000000, gpiote.p_reg->EVENTS_IN[i]);
    }
}

#if !defined(PPR_SKIP_TESTS)
TEST_CASE_DEFINE(nrfx_gpiote, test_case_verify_that_uninit_returns_gpio_gpiote_to_initial_state)
{
#if defined(GPIOTE_FIXED_IN_CH) || defined(GPIOTE_FIXED_OUT_CH)
    // Port event not supported by GPIOTE0 on cpurad
    return;
#endif
    tearDown();
    check_gpio_initial_state();
    check_gpiote_initial_state();

    tearDown();
    setUp();
    _test_case_simple_pin_event_generated_when_connected_output_is_toggled();
    nrfx_gpiote_uninit(&gpiote);
    check_gpio_initial_state();
    check_gpiote_initial_state();

    tearDown();
    setUp();
    nrfx_gpiote_uninit(&gpiote);
    check_gpio_initial_state();
    check_gpiote_initial_state();
}
#endif

TEST_CASE_DEFINE(nrfx_gpiote, test_case_simple_in_value_reading)
{
    nrfx_err_t err;
    nrfx_gpiote_output_config_t out_cfg = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;

#ifdef GPIOTE_FIXED_OUT_CH
    return;
#endif

    err = nrfx_gpiote_output_configure(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN, &out_cfg, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_trigger_config_t trigger_cfg =
    {
        .trigger = NRFX_GPIOTE_TRIGGER_HITOLO
    };
    nrfx_gpiote_handler_config_t handler_cfg =
    {
        .handler = event_handler1
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_cfg,
        .p_handler_config = &handler_cfg,
    };

    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_out_set(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN);

    TEST_ASSERT_TRUE(nrfx_gpiote_in_is_set(NRFX_GPIOTE_TEST_IN_0_PIN));

    nrfx_gpiote_out_clear(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN);

    TEST_ASSERT_FALSE(nrfx_gpiote_in_is_set(NRFX_GPIOTE_TEST_IN_0_PIN));

    nrfx_gpiote_trigger_disable(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN);
}

static void event_handler4(nrfx_gpiote_pin_t     pin,
                           nrfx_gpiote_trigger_t trigger,
                           void *                p_context)
{
    (void)pin;
    (void)trigger;
    (void)p_context;

    m_handler_called++;
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_port_event_short_pulse)
{
    nrfx_err_t err;

#if defined(GPIOTE_FIXED_IN_CH) ||  defined(GPIOTE_FIXED_OUT_CH)
    // Port event or manual pin driving not supported by pins assigned to GPIOTE0 on cpurad
    return;
#endif

    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_0_PIN);

    nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;

    nrfx_gpiote_trigger_config_t trigger_cfg =
    {
        .trigger = NRFX_GPIOTE_TRIGGER_LOTOHI,
    };
    nrfx_gpiote_handler_config_t handler_cfg =
    {
        .handler = event_handler4
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_cfg,
        .p_handler_config = &handler_cfg,
    };

    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, true);

    NRFX_CRITICAL_SECTION_ENTER();
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_test_delay_us(1);
    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_0_PIN);
    NRFX_CRITICAL_SECTION_EXIT();

    // Let the GPIOTE interrupt trigger
    nrf_test_delay_ms(1);

#if defined(NRF_GPIO_LATCH_PRESENT)
    TEST_ASSERT_EQUAL(1, m_handler_called);
#else
    // Without LATCH functionality short pulses cannot be captured
    TEST_ASSERT_EQUAL(0, m_handler_called);
#endif
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_port_event_short_pulse_both_edges)
{
    nrfx_err_t err;

#if defined(GPIOTE_FIXED_IN_CH) ||  defined(GPIOTE_FIXED_OUT_CH)
    // Port event or manual pin driving not supported by pins assigned to GPIOTE0 on cpurad
    return;
#endif

    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_0_PIN);

    nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;

    nrfx_gpiote_trigger_config_t trigger_cfg =
    {
        .trigger = NRFX_GPIOTE_TRIGGER_TOGGLE,
    };
    nrfx_gpiote_handler_config_t handler_cfg =
    {
        .handler = event_handler4
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_cfg,
        .p_handler_config = &handler_cfg,
    };

    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, true);

    NRFX_CRITICAL_SECTION_ENTER();
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_test_delay_us(1);
    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_0_PIN);
    NRFX_CRITICAL_SECTION_EXIT();

    // Let the GPIOTE interrupt trigger
    nrf_test_delay_ms(1);

#if defined(NRF_GPIO_LATCH_PRESENT)
    TEST_ASSERT_EQUAL(2, m_handler_called);
#else
    // Without LATCH functionality short pulses cannot be captured
    TEST_ASSERT_EQUAL(0, m_handler_called);
#endif
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_port_event_many_edges)
{
#if defined(GPIOTE_FIXED_IN_CH) ||  defined(GPIOTE_FIXED_OUT_CH)
    // Port event or manual pin driving not supported by pins assigned to GPIOTE0 on cpurad
    return;
#endif
    nrfx_err_t err;

    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_1_PIN);
    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_1_PIN);

    nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;

    nrfx_gpiote_trigger_config_t trigger_cfg =
    {
        .trigger = NRFX_GPIOTE_TRIGGER_TOGGLE,
    };
    nrfx_gpiote_handler_config_t handler_cfg =
    {
        .handler = event_handler4
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_cfg,
        .p_handler_config = &handler_cfg,
    };

    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_1_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, true);
    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_1_PIN, true);

    // Trigger GPIOTE interrupt on first pin
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_test_delay_ms(1);
    TEST_ASSERT_EQUAL(1, m_handler_called);
    m_handler_called = 0;

    // Trigger GPIOTE interrupt on second pin
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_1_PIN);
    nrf_test_delay_ms(1);
    TEST_ASSERT_EQUAL(1, m_handler_called);
    m_handler_called = 0;

    // Trigger GPIOTE interrupt both pins
    NRFX_CRITICAL_SECTION_ENTER();
    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_1_PIN);
    NRFX_CRITICAL_SECTION_EXIT();
    nrf_test_delay_ms(1);
    TEST_ASSERT_EQUAL(2, m_handler_called);
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_port_event_edge_on_second_pin_while_first_still_asserted)
{
    nrfx_err_t err;
#if defined(GPIOTE_FIXED_IN_CH) ||  defined(GPIOTE_FIXED_OUT_CH)
    // Port event or manual pin driving not supported by pins assigned to GPIOTE0 on cpurad
    return;
#endif

    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_1_PIN);
    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_2_PIN);
    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_1_PIN);
    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_2_PIN);

    nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;

    nrfx_gpiote_trigger_config_t trigger_cfg =
    {
        .trigger = NRFX_GPIOTE_TRIGGER_LOTOHI,
    };
    nrfx_gpiote_handler_config_t handler_cfg =
    {
        .handler = event_handler4
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_cfg,
        .p_handler_config = &handler_cfg,
    };

    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_1_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    trigger_cfg.trigger = NRFX_GPIOTE_TRIGGER_TOGGLE;
    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_2_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, true);
    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_1_PIN, true);
    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_2_PIN, true);

    // Trigger GPIOTE interrupt on first pin
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_test_delay_ms(1);
    TEST_ASSERT_EQUAL(1, m_handler_called);
    m_handler_called = 0;

    // Trigger GPIOTE interrupt on second pin
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_1_PIN);
    nrf_test_delay_ms(1);
    TEST_ASSERT_EQUAL(1, m_handler_called);
    m_handler_called = 0;

    // Trigger two GPIOTE interrupts on third pin
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_2_PIN);
    nrf_test_delay_ms(1);
    TEST_ASSERT_EQUAL(1, m_handler_called);
    m_handler_called = 0;

    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_2_PIN);
    nrf_test_delay_ms(1);
    TEST_ASSERT_EQUAL(1, m_handler_called);
}

static nrfx_gpiote_pin_t dynamic_pin_in;
static nrfx_gpiote_pin_t dynamic_pin_out;

static void event_handler5(nrfx_gpiote_pin_t     pin,
                           nrfx_gpiote_trigger_t trigger,
                           void *                p_context)
{
    (void)pin;
    (void)trigger;
    (void)p_context;
    static bool once;

    if (pin == dynamic_pin_in && once == false)
    {
        nrf_gpio_pin_clear(dynamic_pin_out);
        once = true;
    }
    if (pin != dynamic_pin_in && once == true)
    {
        nrf_gpio_pin_set(dynamic_pin_out);
    }
    m_handler_called++;
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_port_event_repeat_functionality_only_rising_edges)
{
    nrfx_err_t err;

#if defined(GPIOTE_FIXED_IN_CH) ||  defined(GPIOTE_FIXED_OUT_CH)
    // Port event or manual pin driving not supported by pins assigned to GPIOTE0 on cpurad
    return;
#endif

    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_1_PIN);
    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_2_PIN);
    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_1_PIN);
    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_2_PIN);

    if (NRFX_GPIOTE_TEST_IN_0_PIN < NRFX_GPIOTE_TEST_IN_1_PIN)
    {
        dynamic_pin_in = NRFX_GPIOTE_TEST_IN_0_PIN;
        dynamic_pin_out = NRFX_GPIOTE_TEST_OUT_0_PIN;
    }
    else
    {
        dynamic_pin_in = NRFX_GPIOTE_TEST_IN_1_PIN;
        dynamic_pin_out = NRFX_GPIOTE_TEST_OUT_1_PIN;
    }

    nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_trigger_config_t trigger_cfg =
    {
        .trigger = NRFX_GPIOTE_TRIGGER_LOTOHI,
    };
    nrfx_gpiote_handler_config_t handler_cfg =
    {
        .handler = event_handler5
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_cfg,
        .p_handler_config = &handler_cfg,
    };

    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_1_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_2_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, true);
    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_1_PIN, true);
    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_2_PIN, true);

    m_handler_called = 0;

    NRFX_CRITICAL_SECTION_ENTER();
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_1_PIN);
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_2_PIN);
    NRFX_CRITICAL_SECTION_EXIT();

    nrf_test_delay_ms(1);

#if defined(NRF_GPIO_LATCH_PRESENT)
    TEST_ASSERT_EQUAL(4, m_handler_called);
#else
    // Without LATCH functionality additional edge occuring during interrupt processing
    // will not be captured.
    TEST_ASSERT_EQUAL(3, m_handler_called);
#endif
}

void event_handler6(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t action, void * p_context)
{
    (void)action;
    (void)p_context;

    if (m_handler_called == 1)
    {
        nrfx_gpiote_trigger_disable(&gpiote, pin);
    }

    m_handler_called++;
}

/* APIv2 tests */
TEST_CASE_DEFINE(nrfx_gpiote, test_case_level_interrupt)
{
    nrfx_err_t err;

#if defined(GPIOTE_FIXED_IN_CH) ||  defined(GPIOTE_FIXED_OUT_CH)
    // Port event or manual pin driving not supported by pins assigned to GPIOTE0 on cpurad
    return;
#endif

    static const nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    static const nrfx_gpiote_output_config_t output_config = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;
    nrfx_gpiote_trigger_config_t trigger_config = {
        .trigger = NRFX_GPIOTE_TRIGGER_LOW
    };
    nrfx_gpiote_handler_config_t handler_config = {
        .handler = event_handler6
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_config,
        .p_handler_config = &handler_config,
    };

    m_handler_called = 0;

    err = nrfx_gpiote_output_configure(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, true);
    nrf_test_delay_us(1);

    TEST_ASSERT_EQUAL(2, m_handler_called);

    /* Interrupt was disabled in the handler. Enable it again for level high.
     * Interrupt should not be triggered since output pin is low. */
    trigger_config.trigger = NRFX_GPIOTE_TRIGGER_HIGH;

    config.p_pull_config = NULL;
    config.p_handler_config = NULL;
    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, true);

    TEST_ASSERT_EQUAL(2, m_handler_called);

    /* Set pin, this should result in level interrupt. */
    m_handler_called = 0;
    nrfx_gpiote_out_set(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_test_delay_us(1);

    TEST_ASSERT_EQUAL(2, m_handler_called);
}

static void event_handler7(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t action, void * p_context)
{
    (void)pin;
    (void)action;
    (void)p_context;

    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_0_PIN);

    m_handler_called++;
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_level_interrupt2)
{
    nrfx_err_t err;
#if defined(GPIOTE_FIXED_IN_CH) ||  defined(GPIOTE_FIXED_OUT_CH)
    // Port event or manual pin driving not supported by pins assigned to GPIOTE0 on cpurad
    return;
#endif
    static const nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_trigger_config_t trigger_config = {
        .trigger = NRFX_GPIOTE_TRIGGER_HIGH
    };
    nrfx_gpiote_handler_config_t handler_config = {
        .handler = event_handler7
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_config,
        .p_handler_config = &handler_config,
    };

    m_handler_called = 0;

    nrf_gpio_pin_clear(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_0_PIN);

    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, true);
    nrf_test_delay_us(1);

    TEST_ASSERT_EQUAL(0, m_handler_called);

    /* Setting pin should trigger level interrupt. In the interrupt handler pin
     * is cleared. */
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_0_PIN);

    nrf_test_delay_us(1);
    TEST_ASSERT_EQUAL(1, m_handler_called);

    /* Another pin setting results in the interrupt handler call. */
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_0_PIN);

    nrf_test_delay_us(1);
    TEST_ASSERT_EQUAL(2, m_handler_called);
}

static void event_handler8(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t action, void * p_context)
{
    (void)pin;
    (void)action;
    (void)p_context;

    if (pin == NRFX_GPIOTE_TEST_IN_0_PIN)
    {
        nrfx_gpiote_trigger_disable(&gpiote, pin);
    }
    m_handler_called++;
}

static void event_handler9(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t action, void * p_context)
{
    (void)pin;
    (void)action;
    (void)p_context;

    nrfx_gpiote_trigger_enable(p_gpiote0, NRFX_GPIOTE_TEST_IN_0_PIN, true);
    nrf_test_delay_us(1);

    nrfx_gpiote_trigger_disable(&gpiote, pin);
    m_handler_called++;
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_level_interrupt3)
{
    nrfx_err_t err;
#if defined(GPIOTE_FIXED_IN_CH) ||  defined(GPIOTE_FIXED_OUT_CH)
    // Port event or manual pin driving not supported by pins assigned to GPIOTE0 on cpurad
    return;
#endif
    static const nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_trigger_config_t trigger_config = {
        .trigger = NRFX_GPIOTE_TRIGGER_HIGH
    };
    nrfx_gpiote_handler_config_t handler_config = {
        .handler = event_handler9
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_config,
        .p_handler_config = &handler_config,
    };

    m_handler_called = 0;

    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_2_PIN);
    nrf_gpio_cfg_output(NRFX_GPIOTE_TEST_OUT_1_PIN);

    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_1_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    handler_config.handler = event_handler8;
    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_0_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    trigger_config.trigger = NRFX_GPIOTE_TRIGGER_LOTOHI;
    err = nrfx_gpiote_input_configure(&gpiote, NRFX_GPIOTE_TEST_IN_2_PIN, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_1_PIN, true);
    nrfx_gpiote_trigger_enable(&gpiote, NRFX_GPIOTE_TEST_IN_2_PIN, true);

    NRFX_CRITICAL_SECTION_ENTER();
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_1_PIN);
    nrf_gpio_pin_set(NRFX_GPIOTE_TEST_OUT_2_PIN);
    NRFX_CRITICAL_SECTION_EXIT();
    nrf_test_delay_us(1);

    TEST_ASSERT_EQUAL(3, m_handler_called);
}

/* Validates that all gpiote channels configuration is set to default. */
static bool validate_gpiote_idle(void)
{
    for (uint32_t i = 0; i < channels_number; i++)
    {
        if (gpiote.p_reg->CONFIG[i])
        {
            return false;
        }
    }

    return true;
}

/* Validates that pin configuration is set to default. */
static bool validate_gpio_default(uint32_t pin_number)
{
    NRF_GPIO_Type * reg = nrf_gpio_pin_port_decode(&pin_number);

    return (reg->PIN_CNF[pin_number] & ~0x2UL) == 0;
}

/* Validate that number of allocated channels is as expected. Returns false if
 * it is unexpected.
 */
static bool validate_busy_channels(uint32_t exp_busy_cnt)
{
    uint8_t ch[channels_number];
    nrfx_err_t err = NRFX_SUCCESS;
    uint32_t i;

    for (i = 0; i < (channels_number - exp_busy_cnt); i++)
    {
        err = nrfx_gpiote_channel_alloc(&gpiote, &ch[i]);
        if (err != NRFX_SUCCESS)
        {
            break;
        }
    }

    if (err == NRFX_SUCCESS)
    {
        uint8_t tmp;

        err = nrfx_gpiote_channel_alloc(&gpiote, &tmp);
        if (err != NRFX_ERROR_NO_MEM)
        {
            return false;
        }

        err = NRFX_SUCCESS;
    }

    for (uint32_t j = 0; j < i; j++)
    {
        err = nrfx_gpiote_channel_free(&gpiote, ch[j]);
        if (err != NRFX_SUCCESS)
        {
            return false;
        }
    }

    return err == NRFX_SUCCESS;
}

static nrf_gpio_pin_drive_t get_drive(uint32_t pin_number)
{
    NRF_GPIO_Type * reg = nrf_gpio_pin_port_decode(&pin_number);

#if defined(GPIO_PIN_CNF_DRIVE_Msk)
    uint32_t drive_msk = GPIO_PIN_CNF_DRIVE_Msk;
#else
    uint32_t drive_msk = GPIO_PIN_CNF_DRIVE0_Msk | GPIO_PIN_CNF_DRIVE1_Msk;
#endif

    return (nrf_gpio_pin_drive_t)((reg->PIN_CNF[pin_number] & drive_msk) >> GPIO_PIN_CNF_DRIVE_Pos);
}

static nrf_gpio_pin_input_t get_input_connect(uint32_t pin_number)
{
    NRF_GPIO_Type * reg = nrf_gpio_pin_port_decode(&pin_number);

    return (nrf_gpio_pin_input_t)((reg->PIN_CNF[pin_number] & GPIO_PIN_CNF_INPUT_Msk) >>
                                        GPIO_PIN_CNF_INPUT_Pos);
}

static void disconnect_pin(uint32_t pin)
{
    nrfx_err_t err;
    static const nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = NULL,
        .p_handler_config = NULL,
    };

    nrfx_gpiote_trigger_disable(&gpiote, pin);

    err = nrfx_gpiote_input_configure(&gpiote, pin, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    err = nrfx_gpiote_pin_uninit(&gpiote, pin);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    TEST_ASSERT_TRUE(validate_busy_channels(0));
    TEST_ASSERT_TRUE(validate_gpiote_idle());
    TEST_ASSERT_TRUE(validate_gpio_default(pin));
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_gpio_out_pin_config)
{
    nrfx_gpiote_output_config_t output_config = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;
    nrfx_err_t err;
#if defined(GPIOTE_FIXED_IN_CH) || defined(GPIOTE_FIXED_OUT_CH)
    // Port event not supported by GPIOTE0 on cpurad
    return;
#endif

    output_config.drive = NRF_GPIO_PIN_H0S1;
    err = nrfx_gpiote_output_configure(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    TEST_ASSERT_EQUAL(output_config.drive, get_drive(NRFX_GPIOTE_TEST_OUT_0_PIN));
    TEST_ASSERT_EQUAL(output_config.input_connect, get_input_connect(NRFX_GPIOTE_TEST_OUT_0_PIN));

    output_config.drive = NRF_GPIO_PIN_D0S1;
    output_config.input_connect = NRF_GPIO_PIN_INPUT_CONNECT;
    err = nrfx_gpiote_output_configure(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    TEST_ASSERT_EQUAL(output_config.drive, get_drive(NRFX_GPIOTE_TEST_OUT_0_PIN));
    TEST_ASSERT_EQUAL(output_config.input_connect, get_input_connect(NRFX_GPIOTE_TEST_OUT_0_PIN));
}

/* Test validates behavior of the pin when configured as output with GPIOTE task. */
TEST_CASE_DEFINE(nrfx_gpiote, test_case_gpiote_out_pin_config)
{
    nrfx_err_t err;
    uint8_t ch;
    uint32_t task_addr;

#if defined(GPIOTE_FIXED_IN_CH) || defined(GPIOTE_FIXED_OUT_CH)
    // Port event and manual pin driving not supported by GPIOTE0 on cpurad
    return;
#endif

    TEST_ASSERT_TRUE(validate_busy_channels(0));
    nrf_gpio_cfg_input(NRFX_GPIOTE_TEST_IN_0_PIN, NRF_GPIO_PIN_NOPULL);

    /* Initialize pin as output with init low */
    err = nrfx_gpiote_channel_alloc(&gpiote, &ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    TEST_ASSERT_TRUE(validate_busy_channels(1));

    nrfx_gpiote_output_config_t output_config = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;
    nrfx_gpiote_task_config_t task_config = {
        .task_ch = ch,
        .polarity = NRF_GPIOTE_POLARITY_TOGGLE,
        .init_val = NRF_GPIOTE_INITIAL_VALUE_LOW
    };

    err = nrfx_gpiote_output_configure(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN, &output_config, &task_config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Task not enabled by default. No pin change after triggering task. */
    task_addr = nrfx_gpiote_out_task_address_get(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_test_delay_us(1);
    *(volatile uint32_t *)task_addr = 1;

    TEST_ASSERT_FALSE(nrf_gpio_pin_read(NRFX_GPIOTE_TEST_IN_0_PIN));

    /* Enable to activate task. */
    nrfx_gpiote_out_task_enable(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN);
    /* Pin is initilize to low state, toggle to get low. */
    task_addr = nrfx_gpiote_out_task_address_get(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN);
    *(volatile uint32_t *)task_addr = 1;
    nrf_test_delay_us(1);
    TEST_ASSERT_TRUE(nrf_gpio_pin_read(NRFX_GPIOTE_TEST_IN_0_PIN));

    /* Disable gpiote task. */
    task_config.polarity = NRF_GPIOTE_POLARITY_NONE;

    err = nrfx_gpiote_output_configure(p_gpiote0, NRFX_GPIOTE_TEST_OUT_0_PIN, NULL, &task_config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Validate that disconnect does not free the channel */
    disconnect_pin(NRFX_GPIOTE_TEST_OUT_0_PIN);
    TEST_ASSERT_TRUE(validate_busy_channels(1));

    nrfx_gpiote_channel_free(&gpiote, ch);
}

/* Validate configuration of output pin with input connected. */
TEST_CASE_DEFINE(nrfx_gpiote, test_case_gpio_inout_pin_config)
{
    nrfx_err_t err;
#if defined(GPIOTE_FIXED_IN_CH) || defined(GPIOTE_FIXED_OUT_CH)
    // Port event not supported by GPIOTE0 on cpurad
    return;
#endif
    nrfx_gpiote_output_config_t output_config = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;

    output_config.input_connect = NRF_GPIO_PIN_INPUT_CONNECT;


    TEST_ASSERT_TRUE(validate_busy_channels(0));
    nrf_gpio_cfg_input(NRFX_GPIOTE_TEST_IN_0_PIN, NRF_GPIO_PIN_NOPULL);

    err = nrfx_gpiote_output_configure(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    TEST_ASSERT_FALSE(nrf_gpio_pin_read(NRFX_GPIOTE_TEST_OUT_0_PIN));
    TEST_ASSERT_FALSE(nrf_gpio_pin_read(NRFX_GPIOTE_TEST_IN_0_PIN));

    nrfx_gpiote_out_set(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_test_delay_us(1);
    TEST_ASSERT_TRUE(nrf_gpio_pin_read(NRFX_GPIOTE_TEST_OUT_0_PIN));
    TEST_ASSERT_TRUE(nrf_gpio_pin_read(NRFX_GPIOTE_TEST_IN_0_PIN));

    nrfx_gpiote_out_clear(&gpiote, NRFX_GPIOTE_TEST_OUT_0_PIN);
    nrf_test_delay_us(1);
    TEST_ASSERT_FALSE(nrf_gpio_pin_read(NRFX_GPIOTE_TEST_OUT_0_PIN));
    TEST_ASSERT_FALSE(nrf_gpio_pin_read(NRFX_GPIOTE_TEST_IN_0_PIN));
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_in_pin_config)
{
#if defined(GPIOTE_FIXED_IN_CH) || defined(GPIOTE_FIXED_OUT_CH)
    // Port event not supported by GPIOTE0 on cpurad
    return;
#endif
    nrfx_err_t err;
    nrfx_gpiote_pin_t in = NRFX_GPIOTE_TEST_IN_0_PIN;
    nrfx_gpiote_pin_t out = NRFX_GPIOTE_TEST_OUT_0_PIN;
    nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = NULL,
        .p_handler_config = NULL,
    };

    nrf_gpio_cfg_output(out);
    nrf_gpio_pin_clear(out);

    err = nrfx_gpiote_input_configure(&gpiote, in, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    TEST_ASSERT_EQUAL(NRF_GPIO_PIN_NOPULL, nrf_gpio_pin_pull_get(in));
    TEST_ASSERT_FALSE(nrfx_gpiote_in_is_set(in));

    nrf_gpio_pin_set(out);
    TEST_ASSERT_TRUE(nrfx_gpiote_in_is_set(in));

    pull_cfg = NRF_GPIO_PIN_PULLUP;
    err = nrfx_gpiote_input_configure(&gpiote, in, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    TEST_ASSERT_EQUAL(NRF_GPIO_PIN_PULLUP, nrf_gpio_pin_pull_get(in));

    pull_cfg = NRF_GPIO_PIN_PULLDOWN;
    err = nrfx_gpiote_input_configure(&gpiote, in, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    TEST_ASSERT_EQUAL(NRF_GPIO_PIN_PULLDOWN, nrf_gpio_pin_pull_get(in));
}

typedef struct
{
    uint32_t cnt;
    nrfx_gpiote_trigger_t exp_trigger[10];
    nrfx_gpiote_pin_t exp_pin[10];
} pin_handler_data_t;

static void pin_cb(nrfx_gpiote_pin_t     pin,
                   nrfx_gpiote_trigger_t trigger,
                   void *                p_context)
{
    pin_handler_data_t *data = (pin_handler_data_t *)p_context;

    TEST_ASSERT_EQUAL_UINT8(data->exp_pin[data->cnt], pin);
    TEST_ASSERT_EQUAL(data->exp_trigger[data->cnt], trigger);
    data->cnt++;
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_global_callback)
{
#ifdef GPIOTE_FIXED_OUT_CH
    return;
#endif

    uint8_t ch;
    nrfx_err_t err;
    pin_handler_data_t global_data = {0};
    pin_handler_data_t cb_data = {0};

    nrfx_gpiote_pin_t in0 = NRFX_GPIOTE_TEST_IN_0_PIN;
    nrfx_gpiote_pin_t out0 = NRFX_GPIOTE_TEST_OUT_0_PIN;
    nrfx_gpiote_pin_t in1 = NRFX_GPIOTE_TEST_IN_1_PIN;
    nrfx_gpiote_pin_t out1 = NRFX_GPIOTE_TEST_OUT_1_PIN;

    nrf_gpio_cfg_output(out0);
    nrf_gpio_pin_clear(out0);
    nrf_gpio_cfg_output(out1);
    nrf_gpio_pin_clear(out1);

    nrfx_gpiote_global_callback_set(&gpiote, pin_cb, &global_data);
    if (p_gpiote0 != &gpiote)
    {
        nrfx_gpiote_global_callback_set(p_gpiote0, pin_cb, &global_data);
    }

    nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_trigger_config_t trigger_config = {
        .trigger = NRFX_GPIOTE_TRIGGER_LOTOHI,
    };
    nrfx_gpiote_handler_config_t handler_config = {
        .handler = pin_cb,
        .p_context = &cb_data
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_config,
        .p_handler_config = &handler_config,
    };

    /* pin 0 uses PORT event */
    err = nrfx_gpiote_input_configure(&gpiote, in1, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(&gpiote, in1, true);

#ifdef GPIOTE_FIXED_IN_CH
    ch = GPIOTE_FIXED_IN_CH;
#else
    err = nrfx_gpiote_channel_alloc(p_gpiote0, &ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
#endif

    /* Pin 1 uses GPIOTE event */
    trigger_config.p_in_channel = &ch;
    handler_config.handler = NULL;
    err = nrfx_gpiote_input_configure(p_gpiote0, in0, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(p_gpiote0, in0, true);

    TEST_ASSERT_EQUAL(0, global_data.cnt);
    TEST_ASSERT_EQUAL(0, cb_data.cnt);
    global_data.exp_trigger[0] = NRFX_GPIOTE_TRIGGER_LOTOHI;
    global_data.exp_trigger[1] = NRFX_GPIOTE_TRIGGER_LOTOHI;
    global_data.exp_pin[0] = in1;
    global_data.exp_pin[1] = in0;

    cb_data.exp_trigger[0] = NRFX_GPIOTE_TRIGGER_LOTOHI;
    cb_data.exp_pin[0] = in1;

    nrf_gpio_pin_set(out1);
    nrf_test_delay_us(1);
    nrf_gpio_pin_set(out0);
    nrf_test_delay_us(1);

    /* After both pins set there should be 2 global callbacks called and one
     * specific.
     */
    TEST_ASSERT_EQUAL(1, cb_data.cnt);
    TEST_ASSERT_EQUAL(2, global_data.cnt);

    nrfx_gpiote_trigger_disable(&gpiote, in1);
    nrfx_gpiote_trigger_disable(p_gpiote0, in0);
    nrfx_gpiote_channel_free(p_gpiote0, ch);

    nrfx_gpiote_global_callback_set(&gpiote, NULL, NULL);
    if (p_gpiote0 != &gpiote)
    {
        nrfx_gpiote_global_callback_set(p_gpiote0, NULL, NULL);
    }
}

/* Validates managing of resources for pin specific callbacks. */
TEST_CASE_DEFINE(nrfx_gpiote, test_case_pin_specific_callback_allocation)
{
    nrfx_err_t err;
    nrfx_gpiote_pin_t in0 = NRFX_GPIOTE_TEST_IN_0_PIN;
    nrfx_gpiote_pin_t in1 = NRFX_GPIOTE_TEST_IN_1_PIN;
    nrfx_gpiote_pin_t in2 = NRFX_GPIOTE_TEST_IN_2_PIN;
    nrfx_gpiote_pin_t in3 = NRFX_GPIOTE_TEST_OUT_0_PIN;
    nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_handler_config_t handler_config = {
        .handler = pin_cb,
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = NULL,
        .p_handler_config = NULL,
    };

    /* Test relies on given configuration. */
    TEST_ASSERT_EQUAL(NRFX_GPIOTE_CONFIG_NUM_OF_EVT_HANDLERS, 3);

    err = nrfx_gpiote_input_configure(&gpiote, in0, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    err = nrfx_gpiote_input_configure(&gpiote, in1, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    err = nrfx_gpiote_input_configure(&gpiote, in2, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    err = nrfx_gpiote_input_configure(&gpiote, in3, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Successfully assign 3 pin specific handlers. For each pin different p_context
     * is used to ensure that handlers are not reused. */
    config.p_pull_config = NULL,
    config.p_handler_config = &handler_config,

    handler_config.p_context = (void *)0;
    err = nrfx_gpiote_input_configure(&gpiote, in0, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    handler_config.p_context = (void *)1;
    err = nrfx_gpiote_input_configure(&gpiote, in1, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    handler_config.p_context = (void *)2;
    err = nrfx_gpiote_input_configure(&gpiote, in2, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* There is no room for more with given configuration. */
    handler_config.p_context = (void *)3;
    err = nrfx_gpiote_input_configure(&gpiote, in3, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_ERROR_NO_MEM, err);

    /* Uninit pin1 which allows allocation for pin2 */
    err = nrfx_gpiote_pin_uninit(&gpiote, in1);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Successful pin assignment */
    err = nrfx_gpiote_input_configure(&gpiote, in3, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Attempt to allocate another one fails. */
    handler_config.p_context = (void *)1;
    err = nrfx_gpiote_input_configure(&gpiote, in1, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_ERROR_NO_MEM, err);

    /* Unsetting handler for in3 pin. */
    handler_config.handler = NULL;
    err = nrfx_gpiote_input_configure(&gpiote, in3, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Attempt to allocate with success because unsetting released on handler slot. */
    handler_config.handler = pin_cb;
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
}

/* Validates that if same handler, p_context is used for different pin then handler
 * is reused which allows to allocate more individual handlers. */
TEST_CASE_DEFINE(nrfx_gpiote, test_case_pin_callback_reuse)
{
#if defined(GPIOTE_FIXED_IN_CH) || defined(GPIOTE_FIXED_OUT_CH)
    // Port event not supported by GPIOTE0 on cpurad
    return;
#endif
    nrfx_err_t err;
    nrfx_gpiote_pin_t in0 = NRFX_GPIOTE_TEST_IN_0_PIN;
    nrfx_gpiote_pin_t in1 = NRFX_GPIOTE_TEST_IN_1_PIN;
    nrfx_gpiote_pin_t in2 = NRFX_GPIOTE_TEST_IN_2_PIN;
    nrfx_gpiote_pin_t in3 = NRFX_GPIOTE_TEST_OUT_0_PIN;
    nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_handler_config_t handler_config = {
        .handler = pin_cb,
    };
    nrfx_gpiote_trigger_config_t trigger_config = {
        .trigger = NRFX_GPIOTE_TRIGGER_LOTOHI
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_config,
        .p_handler_config = NULL,
    };

    /* Test relies on given configuration. */
    TEST_ASSERT_EQUAL(NRFX_GPIOTE_CONFIG_NUM_OF_EVT_HANDLERS, 3);

    err = nrfx_gpiote_input_configure(&gpiote, in0, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    err = nrfx_gpiote_input_configure(&gpiote, in1, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    err = nrfx_gpiote_input_configure(&gpiote, in2, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    err = nrfx_gpiote_input_configure(&gpiote, in3, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Successfully assign 3 pin specific handlers. For two pins use same handler,p_context.
     * It will result in reusing handler entry and forth pin specific handler will
     * be allocated.
     */
    config.p_pull_config = NULL,
    config.p_trigger_config = NULL,
    config.p_handler_config = &handler_config,

    handler_config.p_context = (void *)0;
    err = nrfx_gpiote_input_configure(&gpiote, in0, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    handler_config.p_context = (void *)1;
    err = nrfx_gpiote_input_configure(&gpiote, in1, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Same handler,p_context as for in0 */
    handler_config.p_context = (void *)0;
    err = nrfx_gpiote_input_configure(&gpiote, in2, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* There is room for one more callback. */
    handler_config.p_context = (void *)3;
    err = nrfx_gpiote_input_configure(&gpiote, in3, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Attempt to change in2 callback and use unique p_context which will require
     * unique handler allocation that shall fail as all 3 handlers are already in use
     */
    handler_config.p_context = (void *)2;
    err = nrfx_gpiote_input_configure(&gpiote, in2, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_ERROR_NO_MEM, err);
}

static void event_handler10(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t action, void * p_context)
{
    (void)pin;
    (void)action;
    (void)p_context;

    m_handler_called++;
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_output_with_trigger)
{
#if defined(GPIOTE_FIXED_IN_CH) || defined(GPIOTE_FIXED_OUT_CH)
    // Port event not supported by GPIOTE0 on cpurad
    return;
#endif
    nrfx_err_t err;
    nrfx_gpiote_pin_t pin = NRFX_GPIOTE_TEST_OUT_0_PIN;
    nrfx_gpiote_output_config_t output_config = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;
    output_config.input_connect = NRF_GPIO_PIN_INPUT_CONNECT;

    nrfx_gpiote_task_config_t task_config = {
        .polarity = NRF_GPIOTE_POLARITY_TOGGLE,
        .init_val = NRF_GPIOTE_INITIAL_VALUE_LOW
    };
    nrfx_gpiote_trigger_config_t trigger_config = {
        .trigger = NRFX_GPIOTE_TRIGGER_LOTOHI
    };
    nrfx_gpiote_handler_config_t handler_config = {
        .handler = event_handler10
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = NULL,
        .p_trigger_config = &trigger_config,
        .p_handler_config = &handler_config,
    };

    err = nrfx_gpiote_channel_alloc(&gpiote, &task_config.task_ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Configure pin as output with input connect and validate that trigger is
     * called when pin changes state. */
    err = nrfx_gpiote_output_configure(&gpiote, pin, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    err = nrfx_gpiote_input_configure(&gpiote, pin, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(&gpiote, pin, true);

    TEST_ASSERT_EQUAL(m_handler_called, 0);

    nrfx_gpiote_out_set(&gpiote, pin);
    nrf_test_delay_us(1);
    TEST_ASSERT_EQUAL(m_handler_called, 1);

    nrfx_gpiote_out_clear(&gpiote, pin);

    /* Reconfigure pin to output driven by GPIOTE task and check that trigger
     * works. */
    err = nrfx_gpiote_output_configure(&gpiote, pin, &output_config, &task_config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_out_task_enable(&gpiote, pin);

    err = nrfx_gpiote_input_configure(&gpiote, pin, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(&gpiote, pin, true);

    TEST_ASSERT_EQUAL(m_handler_called, 1);

    *(volatile uint32_t *)nrfx_gpiote_out_task_address_get(&gpiote, pin) = 1;
    nrf_test_delay_us(1);

    TEST_ASSERT_EQUAL(m_handler_called, 2);

    nrfx_gpiote_trigger_disable(&gpiote, pin);
    nrfx_gpiote_channel_free(&gpiote, task_config.task_ch);
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_input_channel_get)
{
#if defined(GPIOTE_FIXED_OUT_CH)
    // Port event not supported by GPIOTE0 on cpurad
    return;
#endif

    nrfx_err_t err;
    nrfx_gpiote_pin_t pin = NRFX_GPIOTE_TEST_IN_0_PIN;
    uint8_t ch;
    uint8_t exp_ch;

    nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_trigger_config_t trigger_config = {
        .trigger = NRFX_GPIOTE_TRIGGER_LOTOHI
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = NULL,
        .p_handler_config = NULL,
    };

#if defined(GPIOTE_FIXED_IN_CH)
    exp_ch = GPIOTE_FIXED_IN_CH;
#else
    err = nrfx_gpiote_input_configure(p_gpiote0, pin, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    err = nrfx_gpiote_channel_get(p_gpiote0, pin, &ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_ERROR_INVALID_PARAM, err);

    /* In with trigger using PORT */
    config.p_trigger_config = &trigger_config;
    err = nrfx_gpiote_input_configure(p_gpiote0, pin, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    err = nrfx_gpiote_channel_get(p_gpiote0, pin, &ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_ERROR_INVALID_PARAM, err);

    err = nrfx_gpiote_channel_alloc(p_gpiote0, &exp_ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
#endif

    /* Change configuration of the trigger to use GPIOTE channel */
    config.p_pull_config = NULL,
    trigger_config.p_in_channel = &exp_ch;

    err = nrfx_gpiote_input_configure(p_gpiote0, pin, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Valid channel assigned to the pin. */
    err = nrfx_gpiote_channel_get(p_gpiote0, pin, &ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    TEST_ASSERT_EQUAL(exp_ch, ch);

    nrfx_gpiote_channel_free(p_gpiote0, exp_ch);
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_output_channel_get)
{
#if defined(GPIOTE_FIXED_IN_CH)
    return;
#endif

    nrfx_err_t err;
    nrfx_gpiote_pin_t pin = NRFX_GPIOTE_TEST_OUT_0_PIN;
    uint8_t ch;
    static const nrfx_gpiote_output_config_t output_config = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;
    nrfx_gpiote_task_config_t task_config = {
        .polarity = NRF_GPIOTE_POLARITY_TOGGLE,
        .init_val = NRF_GPIOTE_INITIAL_VALUE_LOW
    };

#if defined(GPIOTE_FIXED_OUT_CH)
    task_config.task_ch = GPIOTE_FIXED_OUT_CH;
#else
    err = nrfx_gpiote_channel_alloc(&gpiote, &task_config.task_ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
#endif

    /* Output pin without task has no channel. */
    err = nrfx_gpiote_output_configure(p_gpiote0, pin, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    err = nrfx_gpiote_channel_get(p_gpiote0, pin, &ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_ERROR_INVALID_PARAM, err);

    /* Output pin with task has channel. */
    err = nrfx_gpiote_output_configure(p_gpiote0, pin, NULL, &task_config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    err = nrfx_gpiote_channel_get(p_gpiote0, pin, &ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
    TEST_ASSERT_EQUAL(task_config.task_ch, ch);

#if !defined(GPIOTE_FIXED_OUT_CH)
    nrfx_gpiote_channel_free(p_gpiote0, task_config.task_ch);
#endif
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_reconfigure_input_output)
{
#if defined(GPIOTE_FIXED_IN_CH) || defined(GPIOTE_FIXED_OUT_CH)
    return;
#endif
    nrfx_gpiote_pin_t pin = NRFX_GPIOTE_TEST_OUT_0_PIN;
    uint32_t pin_number = pin;
    NRF_GPIO_Type * reg = nrf_gpio_pin_port_decode(&pin_number);
    static const nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    static const nrfx_gpiote_output_config_t output_config = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;
    nrfx_err_t err;
    uint32_t exp_cfg;
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = NULL,
        .p_handler_config = NULL,
    };

    /* Configure pin as output. */
    err = nrfx_gpiote_output_configure(&gpiote, pin, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    exp_cfg = ((uint32_t)NRF_GPIO_PIN_DIR_OUTPUT << GPIO_PIN_CNF_DIR_Pos)         |
              ((uint32_t)NRF_GPIO_PIN_INPUT_DISCONNECT << GPIO_PIN_CNF_INPUT_Pos) |
              ((uint32_t)NRF_GPIO_PIN_NOPULL << GPIO_PIN_CNF_PULL_Pos)            |
              ((uint32_t)NRF_GPIO_PIN_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)             |
              ((uint32_t)NRF_GPIO_PIN_NOSENSE << GPIO_PIN_CNF_SENSE_Pos);

    TEST_ASSERT_EQUAL_HEX32(exp_cfg, reg->PIN_CNF[pin_number]);

    /* Reconfigure to input. */
    err = nrfx_gpiote_input_configure(&gpiote, pin, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    exp_cfg = ((uint32_t)NRF_GPIO_PIN_DIR_INPUT << GPIO_PIN_CNF_DIR_Pos)       |
              ((uint32_t)NRF_GPIO_PIN_INPUT_CONNECT << GPIO_PIN_CNF_INPUT_Pos) |
              ((uint32_t)NRF_GPIO_PIN_NOPULL << GPIO_PIN_CNF_PULL_Pos)         |
              ((uint32_t)NRF_GPIO_PIN_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)          |
              ((uint32_t)NRF_GPIO_PIN_NOSENSE << GPIO_PIN_CNF_SENSE_Pos);

    TEST_ASSERT_EQUAL_HEX32(exp_cfg, reg->PIN_CNF[pin_number]);

    /* Reconfigure back to output. */
    err = nrfx_gpiote_output_configure(&gpiote, pin, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    exp_cfg = ((uint32_t)NRF_GPIO_PIN_DIR_OUTPUT << GPIO_PIN_CNF_DIR_Pos)         |
              ((uint32_t)NRF_GPIO_PIN_INPUT_DISCONNECT << GPIO_PIN_CNF_INPUT_Pos) |
              ((uint32_t)NRF_GPIO_PIN_NOPULL << GPIO_PIN_CNF_PULL_Pos)            |
              ((uint32_t)NRF_GPIO_PIN_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)             |
              ((uint32_t)NRF_GPIO_PIN_NOSENSE << GPIO_PIN_CNF_SENSE_Pos);

    TEST_ASSERT_EQUAL_HEX32(exp_cfg, reg->PIN_CNF[pin_number]);
}

TEST_CASE_DEFINE(nrfx_gpiote, test_case_reconfigure_input_output2)
{
#if defined(GPIOTE_FIXED_IN_CH) || defined(GPIOTE_FIXED_OUT_CH)
    return;
#endif
    static const nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_output_config_t output_config = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;
    nrfx_gpiote_trigger_config_t trigger_config = {
        .trigger = NRFX_GPIOTE_TRIGGER_LOTOHI
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = &trigger_config,
        .p_handler_config = NULL,
    };
    uint8_t ch;
    nrfx_err_t err;
    nrfx_gpiote_pin_t pin = NRFX_GPIOTE_TEST_OUT_0_PIN;

    err = nrfx_gpiote_channel_alloc(&gpiote, &ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    err = nrfx_gpiote_input_configure(&gpiote, pin, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Cannot configure to output without input connected because output has
     * no input connected.
     */
    err = nrfx_gpiote_output_configure(&gpiote, pin, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_ERROR_INVALID_PARAM, err);

    output_config.input_connect = NRF_GPIO_PIN_INPUT_CONNECT;
    err = nrfx_gpiote_output_configure(&gpiote, pin, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Use GPIOTE event for input. */
    trigger_config.p_in_channel = &ch;
    err = nrfx_gpiote_input_configure(&gpiote, pin, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Fail to reconfigure to output since pin is using event */
    err = nrfx_gpiote_output_configure(&gpiote, pin, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_ERROR_INVALID_PARAM, err);

    /* Disable EVENT trigger. */
    config.p_pull_config = NULL;
    trigger_config.trigger = NRFX_GPIOTE_TRIGGER_NONE;
    err = nrfx_gpiote_input_configure(&gpiote, pin, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Attempt again to configure output with success. */
    err = nrfx_gpiote_output_configure(&gpiote, pin, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);
}
TEST_CASE_DEFINE(nrfx_gpiote, test_case_reconfigure_task_output_input)
{
#if defined(GPIOTE_FIXED_IN_CH) || defined(GPIOTE_FIXED_OUT_CH)
    return;
#endif
    nrfx_gpiote_pin_t pin = NRFX_GPIOTE_TEST_OUT_0_PIN;
    uint32_t pin_number = pin;
    NRF_GPIO_Type * reg = nrf_gpio_pin_port_decode(&pin_number);
    static const nrf_gpio_pin_pull_t pull_cfg = NRFX_GPIOTE_DEFAULT_PULL_CONFIG;
    nrfx_gpiote_output_config_t output_config = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;
    nrfx_err_t err;
    uint32_t exp_cfg;
    uint32_t exp_te_cfg;
    nrfx_gpiote_task_config_t task_config = {
        .polarity = NRF_GPIOTE_POLARITY_HITOLO,
        .init_val = NRF_GPIOTE_INITIAL_VALUE_HIGH
    };
    nrfx_gpiote_input_pin_config_t config = {
        .p_pull_config    = &pull_cfg,
        .p_trigger_config = NULL,
        .p_handler_config = NULL,
    };

    err = nrfx_gpiote_channel_alloc(&gpiote, &task_config.task_ch);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* Configure only pin as output. */
    err = nrfx_gpiote_output_configure(&gpiote, pin, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    exp_cfg = ((uint32_t)NRF_GPIO_PIN_DIR_OUTPUT << GPIO_PIN_CNF_DIR_Pos)         |
              ((uint32_t)NRF_GPIO_PIN_INPUT_DISCONNECT << GPIO_PIN_CNF_INPUT_Pos) |
              ((uint32_t)NRF_GPIO_PIN_NOPULL << GPIO_PIN_CNF_PULL_Pos)            |
              ((uint32_t)NRF_GPIO_PIN_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)             |
              ((uint32_t)NRF_GPIO_PIN_NOSENSE << GPIO_PIN_CNF_SENSE_Pos);

    TEST_ASSERT_EQUAL_HEX32(exp_cfg, reg->PIN_CNF[pin_number]);

    /* At task to the output pin. */
    err = nrfx_gpiote_output_configure(&gpiote, pin, NULL, &task_config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    exp_te_cfg = (pin << GPIOTE_CONFIG_PSEL_Pos) |
                 (task_config.polarity << GPIOTE_CONFIG_POLARITY_Pos) |
                 (task_config.init_val << GPIOTE_CONFIG_OUTINIT_Pos);

    TEST_ASSERT_EQUAL_HEX32(exp_te_cfg, gpiote.p_reg->CONFIG[task_config.task_ch]);

    /* Fail to reconfigure pin to input when task is set. */
    err = nrfx_gpiote_input_configure(&gpiote, pin, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_ERROR_INVALID_PARAM, err);

    /* Trigger can be configured but first input must be connected. */
    output_config.input_connect = NRF_GPIO_PIN_INPUT_CONNECT;
    err = nrfx_gpiote_output_configure(&gpiote, pin, &output_config, NULL);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_config_t trigger_config = {
        .trigger = NRFX_GPIOTE_TRIGGER_HIGH
    };
    config.p_pull_config = NULL,
    config.p_trigger_config = &trigger_config,
    err = nrfx_gpiote_input_configure(&gpiote, pin, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    nrfx_gpiote_trigger_enable(&gpiote, pin, true);

    /* Check that input has been connected. */
    exp_cfg = ((uint32_t)NRF_GPIO_PIN_DIR_OUTPUT << GPIO_PIN_CNF_DIR_Pos)      |
              ((uint32_t)NRF_GPIO_PIN_INPUT_CONNECT << GPIO_PIN_CNF_INPUT_Pos) |
              ((uint32_t)NRF_GPIO_PIN_NOPULL << GPIO_PIN_CNF_PULL_Pos)         |
              ((uint32_t)NRF_GPIO_PIN_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)          |
              ((uint32_t)NRF_GPIO_PIN_SENSE_HIGH << GPIO_PIN_CNF_SENSE_Pos);

    TEST_ASSERT_EQUAL_HEX32(exp_cfg, reg->PIN_CNF[pin_number]);

    /* To reconfigure pin to output task must first be disabled. */
    task_config.polarity = NRF_GPIOTE_POLARITY_NONE;
    err = nrfx_gpiote_output_configure(&gpiote, pin, NULL, &task_config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    /* After that pin can be reconfigured to input. */
    config.p_pull_config = &pull_cfg,
    config.p_trigger_config = NULL,
    err = nrfx_gpiote_input_configure(&gpiote, pin, &config);
    TEST_ASSERT_EQUAL_HEX32(NRFX_SUCCESS, err);

    exp_cfg = ((uint32_t)NRF_GPIO_PIN_DIR_INPUT << GPIO_PIN_CNF_DIR_Pos)       |
              ((uint32_t)NRF_GPIO_PIN_INPUT_CONNECT << GPIO_PIN_CNF_INPUT_Pos) |
              ((uint32_t)NRF_GPIO_PIN_NOPULL << GPIO_PIN_CNF_PULL_Pos)         |
              ((uint32_t)NRF_GPIO_PIN_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)          |
              ((uint32_t)NRF_GPIO_PIN_SENSE_HIGH << GPIO_PIN_CNF_SENSE_Pos);

    TEST_ASSERT_EQUAL_HEX32(exp_cfg, reg->PIN_CNF[pin_number]);
    nrfx_gpiote_trigger_disable(&gpiote, pin);
}

#ifndef CONFIG_ZTEST
case_t m_test_cases[] = {
    TEST_CASE(test_case_ppi_task_can_control_out_task),
    TEST_CASE(test_case_simple_pin_event_generated_when_connected_output_is_toggled),
    TEST_CASE(test_case_simple_pin_event_generated_when_connected_output_is_changed),
    TEST_CASE(test_case_simple_port_event_watcher),
    TEST_CASE(test_case_simple_in_value_reading),
#if !defined(PPR_SKIP_TESTS)
    TEST_CASE(test_case_verify_that_uninit_returns_gpio_gpiote_to_initial_state),
#endif
#if !defined(NRF_SECURE)
    TEST_CASE(test_case_port_event_short_pulse),
    TEST_CASE(test_case_port_event_short_pulse_both_edges),
#endif
    TEST_CASE(test_case_port_event_many_edges),
    TEST_CASE(test_case_port_event_edge_on_second_pin_while_first_still_asserted),
    TEST_CASE(test_case_port_event_repeat_functionality_only_rising_edges),
    /* API v2 test cases */
    TEST_CASE(test_case_level_interrupt),
    TEST_CASE(test_case_level_interrupt2),
    TEST_CASE(test_case_level_interrupt3),
    TEST_CASE(test_case_gpio_out_pin_config),
    TEST_CASE(test_case_gpiote_out_pin_config),
    TEST_CASE(test_case_gpio_inout_pin_config),
    TEST_CASE(test_case_in_pin_config),
    TEST_CASE(test_case_global_callback),
    TEST_CASE(test_case_pin_specific_callback_allocation),
    TEST_CASE(test_case_pin_callback_reuse),
    TEST_CASE(test_case_output_with_trigger),
    TEST_CASE(test_case_input_channel_get),
    TEST_CASE(test_case_output_channel_get),
    TEST_CASE(test_case_reconfigure_input_output),
    TEST_CASE(test_case_reconfigure_input_output2),
    TEST_CASE(test_case_reconfigure_task_output_input),
};
#endif /* !CONFIG_ZTEST */

TEST_SUITE_DEFINE(nrfx_gpiote, setUp, tearDown);

#endif
