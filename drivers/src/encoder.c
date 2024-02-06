#define DT_DRV_COMPAT encoder_counter

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

#include <helpers/nrfx_gppi.h>
#include <nrfx_gpiote.h>
#include <nrfx_timer.h>


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

    // Capture count value in TIMER CC register.
    // Returns count but ignore it because it is recommended to use value from nrfx_timer_capture_get instead
    nrfx_timer_capture(&timer_inst, NRF_TIMER_CC_CHANNEL0);

    // get count value from TIMER CC register
    return (int32_t)nrfx_timer_capture_get(&timer_inst, NRF_TIMER_CC_CHANNEL0);
}

static int encoder_init(const struct device* dev)
{
    const struct encoder_config* config     = dev->config;
    nrfx_timer_t                 timer_inst = config->timer_inst;
    nrfx_gpiote_pin_t            pin        = config->gpio_spec.pin;

    const struct gpio_dt_spec    pin_spec   =
    {
        .pin = config->gpio_spec.pin,
    };

    gpio_pin_configure_dt(&pin_spec, GPIO_INPUT);

    // allocate available channel
    uint8_t gpiote_chan;
    nrfx_gpiote_channel_alloc(&gpiote_chan);


    // const uint8_t * const gppi_ch;
	static const nrfx_gpiote_trigger_config_t trigger_cfg = {
		.trigger = NRFX_GPIOTE_TRIGGER_TOGGLE,
	};

	static const nrfx_gpiote_input_config_t input_config = {
		.pull = NRF_GPIO_PIN_NOPULL,
	};

    nrfx_err_t res = nrfx_gpiote_input_configure(pin, &input_config, &trigger_cfg, NULL);

    // GPIOTE EVENT trigger for ppi. false - no irq
    nrfx_gpiote_trigger_enable(pin, false);

    // Initialize the timer in counter mode
    nrfx_timer_config_t timer_cfg = NRFX_TIMER_DEFAULT_CONFIG(NRF_TIMER_FREQ_16MHz);
    timer_cfg.mode                = NRF_TIMER_MODE_COUNTER;
    res                           = nrfx_timer_init(&timer_inst, &timer_cfg, NULL);
    if (res != NRFX_SUCCESS) {
        return res;
    }

    // connect gpiote event to timer task through channel
    res = nrfx_ppi_channel_assign(gpiote_chan, nrfx_gpiote_in_event_address_get(pin),  nrfx_timer_task_address_get(&timer_inst, NRF_TIMER_TASK_COUNT));
    if (res != NRFX_SUCCESS) {
        return res;
    }

    nrfx_timer_enable(&timer_inst);

    return 0;
}

#define ENCODER_DEFINE(i)                                                                                           \
    static const struct encoder_config encoder_config_##i = {                                                       \
        .timer_inst = {                                                                                             \
            .p_reg            = (NRF_TIMER_Type*)DT_PROP(DT_INST(i, encoder_counter), timer_address),               \
            .instance_id      = DT_PROP(DT_INST(i, encoder_counter), timer_id),                                     \
            .cc_channel_count = 4,                                                                                  \
        },                                                                                                          \
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