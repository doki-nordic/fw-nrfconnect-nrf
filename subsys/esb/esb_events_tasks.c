
#define NUMBER_OF_GROUPS 2
#define NUMBER_OF_CHANNELS 3
#define NUMBER_OF_PPI_HELPER_CHANNELS 2
#define NUMBER_OF_3_OUTPUT_CONNECTIONS 2


#define _RADIO_SHORT_NAME(x) | NRF_RADIO_SHORT_ ## x ## _MASK
#define RADIO_SHORTS(...) \
	if (enable_graph) { \
		nrf_radio_shorts_set(NRF_RADIO, 0 \
		MACRO_MAP_CAT(_RADIO_SHORT_NAME, __VA_ARGS__)) \
	}

#define _TIMER_SHORT_NAME(x) | NRF_TIMER_SHORT_ ## x ## _MASK
#define TIMER_SHORTS(...) \
	if (enable_graph) { \
		nrf_timer_shorts_set(esb_timer.p_reg, 0 \
		MACRO_MAP_CAT(_TIMER_SHORT_NAME, __VA_ARGS__)) \
	}


#ifdef CONFIG_NRFX_DPPI

#define TIMER(x) nrf_timer_publish_set(esb_timer.p_reg, NRF_TIMER_EVENT_ ## x, current_channel), \
		 nrf_timer_publish_clear(esb_timer.p_reg, NRF_TIMER_EVENT_ ## x), \
		 nrf_timer_subscribe_set(esb_timer.p_reg, NRF_TIMER_TASK_ ## x, current_channel), \
		 nrf_timer_subscribe_clear(esb_timer.p_reg, NRF_TIMER_TASK_ ## x)

#define RADIO(x) \
	nrf_radio_publish_set(NRF_RADIO, NRF_RADIO_EVENT_ ## x, current_channel), \
	nrf_radio_publish_clear(NRF_RADIO, NRF_RADIO_EVENT_ ## x), \
	nrf_radio_subscribe_set(NRF_RADIO, NRF_RADIO_TASK_ ## x, current_channel), \
	nrf_radio_subscribe_clear(NRF_RADIO, NRF_RADIO_TASK_ ## x)

#define EGU(x) \
	nrf_egu_publish_set(ESB_EGU, NRF_EGU_EVENT_TRIGGERED ## x, current_channel), \
	nrf_egu_publish_clear(ESB_EGU, NRF_EGU_EVENT_TRIGGERED ## x), \
	nrf_egu_subscribe_set(ESB_EGU, NRF_EGU_TASK_TRIGGER ## x, current_channel), \
	nrf_egu_subscribe_clear(ESB_EGU, NRF_EGU_TASK_TRIGGER ## x)

#define ENABLE_GROUP(g) _something_is_wrong_if_used, \
			_something_is_wrong_if_used, \
			nrf_dppi_subscribe_set(ESB_DPPIC, nrf_dppi_group_enable_task_get(esb_groups[g]), current_channel), \
			nrf_dppi_subscribe_clear(ESB_DPPIC, nrf_dppi_group_enable_task_get(esb_groups[g]), current_channel)

#define DISABLE_GROUP(g) _something_is_wrong_if_used, \
			_something_is_wrong_if_used, \
			nrf_dppi_subscribe_set(ESB_DPPIC, nrf_dppi_group_disable_task_get(esb_groups[g]), current_channel), \
			nrf_dppi_subscribe_clear(ESB_DPPIC, nrf_dppi_group_disable_task_get(esb_groups[g]), current_channel)

#define _CONNECT_DPPI(channel, enable, \
	from1_set, from1_clear, _a1, _b1, \
	from2_set, from2_clear, _a2, _b2, \
	from3_set, from3_clear, _a3, _b3, \
	_c1, _d1, to1_set, to1_clear, \
	_c2, _d2, to2_set, to2_clear, \
	_c3, _d3, to3_set, to3_clear) \
	do { \
		uint8_t current_channel = *channel_ptr; \
		channel_ptr++; \
		if (enable_graph) { \
			from1_set; from2_set; from3_set; to1_set; to2_set; to3_set; \
		} else { \
			from1_clear; from2_clear; from3_clear; to1_clear; to2_clear; to3_clear; \
		} \
		if (!enable) { \
			disabled_mask |= BIT(current_channel); \
		} \
		used_mask |= BIT(current_channel); \
	} while (0)


#define CONNECT_1_TO_1(channel, enable, from1, to1) _CONNECT_DPPI(channel, enable, from1, , , , , , , , , to1, , , , , , , , )
#define CONNECT_1_TO_2(channel, enable, from1, to1, to2) _CONNECT_DPPI(channel, enable, from1, , , , , , , , , to1, to2, , , , )
#define CONNECT_1_TO_3(channel, helper, enable, from1, to1, to2, to3) _CONNECT_DPPI(channel, enable, from1, , , , , , , , , to1, to2, to3)
#define CONNECT_2_TO_1(channel, enable, from1, from2, to1) _CONNECT_DPPI(channel, enable, from1, from2, , , , , to1, , , , , , , , )
#define CONNECT_2_TO_2(channel, enable, from1, from2, to1, to2) _CONNECT_DPPI(channel, enable, from1, from2, , , , , to1, to2, , , , )
#define CONNECT_2_TO_3(channel, helper, enable, from1, from2, to1, to2, to3) _CONNECT_DPPI(channel, enable, from1, from2, , , , , to1, to2, to3)
// #define CONNECT_3_TO_1(channel, enable, from1, from2, from3, to1) _CONNECT_DPPI(channel, enable, from1, from2, from3, to1, , , , , , , , )
// #define CONNECT_3_TO_2(channel, enable, from1, from2, from3, to1, to2) _CONNECT_DPPI(channel, enable, from1, from2, from3, to1, to2, , , , )
// #define CONNECT_3_TO_3(channel, enable, from1, from2, from3, to1, to2, to3) _CONNECT_DPPI(channel, enable, from1, from2, from3, to1, to2, to3)


#define GROUP_BEGIN(group_index)                                                                   \
	do {                                                                                       \
		const uint8_t current_group = esb_groups[group_index];                             \
		uint32_t disabled_mask_copy;                                                       \
		uint32_t used_mask_copy;                                                           \
		do {                                                                               \
			uint32_t disabled_mask = 0;                                                \
			uint32_t used_mask = 0;                                                    \

#define GROUP_END() \
			disabled_mask_copy = disabled_mask;                                        \
			used_mask_copy = used_mask;                                                \
		} while (0);                                                                       \
		if (enable_graph) {                                                                \
			nrf_dppi_channels_group_set(ESB_DPPIC, used_mask_copy, current_group);     \
		}                                                                                  \
		used_mask |= used_mask_copy;                                                       \
		disabled_mask |= disabled_mask_copy;                                               \
	} while (0)

#define MANUAL_GROUP_BEGIN(out_mask_pointer)                                                       \
	do {                                                                                       \
		uint32_t *const result = (out_mask_pointer);                                       \
		uint32_t disabled_mask_copy;                                                       \
		uint32_t used_mask_copy;                                                           \
		do {                                                                               \
			uint32_t disabled_mask = 0;                                                \
			uint32_t used_mask = 0;                                                    \

#define MANUAL_GROUP_END()                                                                         \
			disabled_mask_copy = disabled_mask;                                        \
			used_mask_copy = used_mask;                                                \
		} while (0);                                                                       \
		if (enable_graph) {                                                                \
			*result = used_mask_copy;                                                  \
		}                                                                                  \
		used_mask |= used_mask_copy;                                                       \
		disabled_mask |= disabled_mask_copy;                                               \
	} while (0)

#define _GROUP_CHANNEL_BITS(x) | BIT(esb_channels[x])
#define GROUP(g, ...) \
	if (enable_graph) { \
		nrf_dppi_channels_group_set(ESB_DPPIC, 0 MACRO_MAP_CAT(_GROUP_CHANNEL_BITS, __VA_ARGS__), esb_groups[g]); \
	}

#define GRAPH_BEGIN(this_function_name) \
	if (enable_graph) { \
		if (active_graph != NULL) { \
			active_graph(false); \
		} \
		active_graph = this_function_name; \
	} else { \
		nrf_dppi_channels_disable(ESB_DPPIC, esb_all_channels_mask); \
		nrf_radio_shorts_set(NRF_RADIO, 0); \
		nrf_timer_shorts_set(esb_timer.p_reg, 0); \
	} \
	uint32_t enabled_channel_mask = 0; \

#define GRAPH_END() \
	if (enable_graph) { \
		nrf_dppi_channels_enable(ESB_DPPIC, enabled_channel_mask); \
	}


#else


#define TIMER(x) nrf_timer_event_address_get(esb_timer.p_reg, NRF_TIMER_EVENT_ ## x), \
		 nrf_timer_task_address_get(esb_timer.p_reg, NRF_TIMER_TASK_ ## x)


#define RADIO(x) nrf_radio_event_address_get(esb_timer.p_reg, NRF_RADIO_EVENT_ ## x), \
		 nrf_timer_task_address_get(NRF_RADIO, NRF_RADIO_TASK_ ## x)

#define EGU(x) nrf_egu_event_address_get(ESB_EGU, esb_egu_events[x]), \
		nrf_egu_task_address_get(ESB_EGU, esb_egu_events[x])

#define ENABLE_GROUP(g) _something_is_wrong_if_used, \
			nrf_ppi_task_group_enable_address_get(NRF_PPI, esb_groups[g])

#define DISABLE_GROUP(g) _something_is_wrong_if_used, \
			nrf_ppi_task_group_disable_address_get(NRF_PPI, esb_groups[g])

#define _CONNECT_1_TO_1(channel, enable, from1, _a1, _b1, to1) do { \
		uint8_t current_channel = esb_channels[channel]; \
		if (enable_graph) { \
			nrf_ppi_channel_endpoint_setup(NRF_PPI, current_channel, from1, to1); \
		} else { \
			nrf_ppi_channel_endpoint_setup(NRF_PPI, current_channel, 0, 0); \
		} \
		if (enable) { \
		 \	enabled_channel_mask |= BIT(current_channel); \
		}
	} while (0)

#define _CONNECT_1_TO_2(channel, enable, from1, _a1, _b1, to1, _b2, to2) do { \
		uint8_t current_channel = esb_channels[channel]; \
		if (enable_graph) { \
			nrf_ppi_channel_and_fork_endpoint_setup(NRF_PPI, current_channel, from1, to1, to2); \
		} else { \
			nrf_ppi_channel_and_fork_endpoint_setup(NRF_PPI, current_channel, 0, 0, 0); \
		} \
		if (enable) { \
			enabled_channel_mask |= BIT(current_channel); \
		} \
	} while (0)


#define _CONNECT_1_TO_3(channel, helper, enable, from1, _a1, _b1, to1, _b2, to2, _b3, to3) do { \
		uint8_t current_channel = esb_channels[channel]; \
		uint8_t helper_channel = esb_channels[helper]; \
		if (enable_graph) { \
			nrf_ppi_channel_and_fork_endpoint_setup(NRF_PPI, current_channel, from1, to1, to2); \
			nrf_ppi_channel_endpoint_setup(NRF_PPI, helper_channel, from1, to3); \
		} else { \
			nrf_ppi_channel_and_fork_endpoint_setup(NRF_PPI, current_channel, 0, 0, 0); \
			nrf_ppi_channel_endpoint_setup(NRF_PPI, helper_channel, 0, 0); \
		} \
		if (enable) { \
			enabled_channel_mask |= BIT(current_channel) | BIT(helper_channel); \
		} \
	} while (0)


	

#define CONNECT_1_TO_1(channel, enable, from1, to1) _CONNECT_1_TO_1(channel, enable, from1, to1)
#define CONNECT_1_TO_2(channel, enable, from1, to1, to2) _CONNECT_1_TO_2(channel, enable, from1, to1, to2)
#define CONNECT_1_TO_3(channel, helper, enable, from1, to1, to2, to3) _CONNECT_1_TO_3(channel, enable, from1, to1, to2, to3)

#define HELPER(x) ((x) + NUMBER_OF_CHANNELS)


#endif

static void (*active_graph)(bool enable_graph);


void esb_clear_active_graph(void)
{
	if (active_graph != NULL) {
		active_graph(false);
		active_graph = NULL;
	}
}


void esb_graph_fast_sw_ack(bool enable_graph)
{
	GRAPH_BEGIN(esb_graph_fast_sw_ack);

	RADIO_SHORTS(READY, /* -> */ START);
	TIMER_SHORTS(COMPARE1, /* -> */ STOP);

	GROUP_BEGIN(0);

	CONNECT_1_TO_3(true,
		RADIO(PHYEND), /* -> */ DISABLE_GROUP(0),
					ENABLE_GROUP(1),
					RADIO(RXEN));

	GROUP_END();

	GROUP_BEGIN(1);
	
	CONNECT_2_TO_1(false,
		RADIO(END),
		TIMER(COMPARE0), /* -> */ RADIO(DISABLE));

	GROUP_END();

	CONNECT_2_TO_3(true,
		TIMER(COMPARE1),
		EGU(0),          /* -> */ TIMER(CLEAR),
					  TIMER(START),
					  RADIO(TXEN));

	GRAPH_END();
}


void esb_graph_normal_sw_ack(bool enable_graph)
{
	GRAPH_BEGIN(esb_graph_normal_sw_ack);

	RADIO_SHORTS(READY, /* -> */ START,
		     PHYEND, /* -> */ DISABLE,
		     DISABLED, /* -> */ RXEN);
	TIMER_SHORTS(COMPARE1, /* -> */ STOP);

	CONNECT_1_TO_1(true,
		TIMER(COMPARE0), /* -> */ RADIO(DISABLE));

	CONNECT_2_TO_3(true,
		TIMER(COMPARE1),
		EGU(0),          /* -> */ TIMER(CLEAR),
					  TIMER(START),
					  RADIO(TXEN));

	GRAPH_END();
}
