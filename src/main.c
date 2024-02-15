#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(Example_App, LOG_LEVEL_INF);

static struct sensor_value encoder_left_data;
static struct sensor_value encoder_right_data;

static const struct device* encoder_left_dev = DEVICE_DT_GET(DT_NODELABEL(encoder_left));
static const struct device* encoder_right_dev = DEVICE_DT_GET(DT_NODELABEL(encoder_right));

void get_encoder_data()
{
    int ret;
    ret = sensor_channel_get(encoder_left_dev, SENSOR_CHAN_POS_DY, &encoder_left_data);
    if (ret != 0) {
        LOG_INF("sensor_channel_get error: %d", ret);
    }

    ret = sensor_channel_get(encoder_right_dev, SENSOR_CHAN_POS_DY, &encoder_right_data);
    if (ret != 0) {
        LOG_INF("sensor_channel_get error: %d", ret);
    }
}
int main(void)
{
    if (encoder_left_dev == NULL || encoder_left_dev == NULL) {
        LOG_INF("ERROR: encoder not found");
        return -1;
    }
    if (!device_is_ready(encoder_left_dev)) {
        LOG_INF("left encoder not ready");
    } else {
        LOG_INF("left encoder ready");
    }
    if (!device_is_ready(encoder_right_dev)) {
        LOG_INF("right encoder not ready");
    } else {
        LOG_INF("right encoder ready");
    }

    while (1) {
        int ret;
        ret = sensor_channel_get(encoder_left_dev, SENSOR_CHAN_ALL, &encoder_left_data);
        ret = sensor_channel_get(encoder_right_dev, SENSOR_CHAN_ALL, &encoder_right_data);
        LOG_INF("encoder left data %d", encoder_left_data.val1);
        LOG_INF("encoder right data %d", encoder_right_data.val1);
        k_msleep(1000); // Update heading at least every time we get gyro measurments which is 12.5 Hz.
    }

adding stuff to break compilation

    return 0;
}
