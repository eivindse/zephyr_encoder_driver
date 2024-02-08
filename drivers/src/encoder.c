#define DT_DRV_COMPAT encoder_counter

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

#include <helpers/nrfx_gppi.h>
#include <nrfx_ppi.h>
#include <nrfx_gpiote.h>
#include <nrfx_timer.h>

LOG_MODULE_REGISTER(Encoder_Counter, LOG_LEVEL_INF);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct encoder_config {
    struct gpio_dt_spec gpio_spec;
    nrfx_timer_t        timer_inst;
};

static void reset_timer_count(const struct device* dev)
{
    const struct encoder_config* config     = dev->config;
    nrfx_timer_t                 timer_inst = config->timer_inst;

    nrfx_timer_clear(&timer_inst);
}

static int32_t get_timer_count(const struct device* dev)
{
    const struct encoder_config* config     = dev->config;
    nrfx_timer_t                 timer_inst = config->timer_inst;

    // Capture count and return count, but it is recommended to get value with xx_get when using ppi
    nrfx_timer_capture(&timer_inst, NRF_TIMER_CC_CHANNEL0);

    // get count value from TIMER CC register
    return (int32_t)nrfx_timer_capture_get(&timer_inst, NRF_TIMER_CC_CHANNEL0);
}

static int encoder_init(const struct device* dev)
{
    const struct encoder_config* config     = dev->config;
    nrfx_timer_t                 timer_inst = config->timer_inst;
    nrfx_gpiote_pin_t            pin        = config->gpio_spec.pin;
    
    uint8_t gpiote_chan;
    nrfx_err_t err = nrfx_gpiote_channel_alloc(&gpiote_chan);
    if (err != NRFX_SUCCESS) {
        return err;
    }

    const nrfx_gpiote_trigger_config_t trigger_cfg = {
        .trigger = NRFX_GPIOTE_TRIGGER_TOGGLE,
        .p_in_channel = &gpiote_chan,
    };

    const nrfx_gpiote_input_config_t input_config = {
        .pull = NRF_GPIO_PIN_NOPULL,
    };

    LOG_INF("nrfx_gpiote_input_configure");
    // Function for configuring the specified input pin and input event
    err = nrfx_gpiote_input_configure(pin, &input_config, &trigger_cfg, NULL);
    if (err != NRFX_SUCCESS) {
        return err;
    }

    // GPIOTE EVENT trigger for ppi
    nrfx_gpiote_trigger_enable(pin, false);

	const nrfx_timer_config_t timer_cfg = {
		.frequency = NRFX_MHZ_TO_HZ(1UL),
		.mode = NRF_TIMER_MODE_COUNTER,
		.bit_width = NRF_TIMER_BIT_WIDTH_32,
		.interrupt_priority = NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY,
		.p_context = NULL,
	};

    err = nrfx_timer_init(&timer_inst, &timer_cfg, NULL);
    if (err != NRFX_SUCCESS) {
        return err;
    }

    nrf_ppi_channel_t  ppi_chan;
    err = nrfx_ppi_channel_alloc(&ppi_chan);

    // connect gpiote event to timer task through channel
    err = nrfx_ppi_channel_assign(ppi_chan, nrfx_gpiote_in_event_address_get(pin), nrfx_timer_task_address_get(&timer_inst, NRF_TIMER_TASK_COUNT));
    if (err != NRFX_SUCCESS) {
        return err;
    }
    nrf_ppi_channels_enable(NRF_PPI, BIT(ppi_chan));

    nrfx_timer_enable(&timer_inst);

    LOG_INF("encoder initialized");
    return 0;
}

#define ENCODER_DEFINE(i)                                                                                           \
    static const struct encoder_config encoder_config_##i = {                                                       \
        .timer_inst = NRFX_TIMER_INSTANCE(CONFIG_ENCODER##i##_TIMER),                                               \
        .gpio_spec = GPIO_DT_SPEC_INST_GET_OR(i, gpios, { 0 }),                                                     \
    };                                                                                                              \
    static int get_and_reset_count##i(const struct device* dev, enum sensor_channel chan, struct sensor_value* val) \
    {                                                                                                               \
        val->val1 = get_timer_count(dev);                                                                           \
        reset_timer_count(dev);                                                                                     \
        return 0;                                                                                                   \
    }                                                                                                               \
    static const struct sensor_driver_api api##i = {                                                                \
        .channel_get = get_and_reset_count##i,                                                                      \
    };                                                                                                              \
                                                                                                                    \
    SENSOR_DEVICE_DT_INST_DEFINE(i, encoder_init, NULL,                                                             \
        NULL, &encoder_config_##i,                                                                                  \
        POST_KERNEL, 0, &api##i);

DT_INST_FOREACH_STATUS_OKAY(ENCODER_DEFINE)

#endif // DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)