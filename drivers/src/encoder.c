#define DT_DRV_COMPAT encoder_counter

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include <helpers/nrfx_gppi.h>
#include <nrfx_gpiote.h>
#include <nrfx_timer.h>

LOG_MODULE_REGISTER(encoder_counter, CONFIG_BM_LOG_LEVEL);

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
    uint32_t                     pin        = config->gpio_spec.pin;

    uint32_t err;

    // initializing a GPIOTE input pin.
    nrfx_gpiote_in_config_t gpiote_cfg = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);

    err = nrfx_gpiote_in_init(pin, &gpiote_cfg, NULL);
    if (err != NRFX_SUCCESS) {
        LOG_ERR("nrfx_gpiote_in_init failed");
        return err;
    }

    // Function for enabling trigger for the given pin for PPI
    nrfx_gpiote_trigger_enable(pin, false);

    // Initialize the timer in counter mode
    nrfx_timer_config_t timer_cfg = NRFX_TIMER_DEFAULT_CONFIG;
    timer_cfg.mode                = NRF_TIMER_MODE_COUNTER;
    err                           = nrfx_timer_init(&timer_inst, &timer_cfg, NULL);
    if (err != NRFX_SUCCESS) {
        LOG_ERR("nrfx_gpiote_in_init failed");
        return err;
    }

    // Set up a PPI channel to connect the GPIOTE event to the timer COUNT task
    uint8_t gppi_ch;
    err = nrfx_gppi_channel_alloc(&gppi_ch);
    if (err != NRFX_SUCCESS) {
        LOG_ERR("nrfx_gpiote_in_init failed");
        return err;
    }

    // setup PPI from event GPIO EVENT(pin) to task TIMER TASK_COUNT
    nrfx_gppi_channel_endpoints_setup(gppi_ch, nrfx_gpiote_in_event_addr_get(pin), nrfx_timer_task_address_get(&timer_inst, NRF_TIMER_TASK_COUNT));

    // enable the allocated ppi channel and timer
    nrfx_gppi_channels_enable(BIT(gppi_ch));
    nrfx_timer_enable(&timer_inst);

    LOG_INF("%s ready", dev->name);
    return 0;
}

#define ENCODER_DEFINE(i)                                                                                           \
    static const struct encoder_config encoder_config_##i = {                                                       \
        .timer_inst = {                                                                                             \
            .p_reg            = (NRF_TIMER_Type*)DT_PROP(DT_INST(i, encoder_counter), timer_address),         \
            .instance_id      = DT_PROP(DT_INST(i, encoder_counter), timer_id),                               \
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