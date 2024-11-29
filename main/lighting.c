#include "ha/esp_zigbee_ha_standard.h"
#include "driver/ledc.h"

#include "lighting.h"

static const char *TAG = "ZB_DUAL_LIGHT_LIGHTING_C";

const static gpio_num_t pin_map[NUM_LIGHTS] = {
    [MAIN_COOL] = GPIO_NUM_3,
    [MAIN_WARM] = GPIO_NUM_11,
    [AUX_W]     = GPIO_NUM_1,
    [AUX_R]     = GPIO_NUM_7,
    [AUX_G]     = GPIO_NUM_5,
    [AUX_B]     = GPIO_NUM_4,
};

/* Track level, even when the light gets turned off so that we can restore it.
    Technically, we are supposed to be using the StartupOnOff attribute but I
    don't think home assistant is going to care if this implementation is
    broken, based on what messages it's been sending. I havent seen anything
    addressed to the StartupOnOff attribute. Still, broken implementation. */
static unsigned char cool_level =  0xfe;
static unsigned char warm_level =  0xfe;
static unsigned char r_level    =  0xfe;
static unsigned char g_level    =  0xfe;
static unsigned char b_level    =  0xfe;

static void timer_init()
{
    ledc_timer_config_t pwm_conf = {};
	
	pwm_conf.speed_mode = LEDC_LOW_SPEED_MODE;
	pwm_conf.timer_num = 0;
	pwm_conf.freq_hz = 300000;
	pwm_conf.duty_resolution = LEDC_TIMER_8_BIT;
	pwm_conf.clk_cfg = LEDC_USE_PLL_DIV_CLK;

    ledc_timer_config(&pwm_conf);
}

static void channel_init(enum channel chnl)
{
    ledc_channel_config_t chnl_conf = {};

	chnl_conf.gpio_num = pin_map[chnl];
	chnl_conf.channel = chnl;
	chnl_conf.timer_sel = 0;
	chnl_conf.duty = 32; // A nice dim value to show that we've started but nothing has actually set the level

	ledc_channel_config(&chnl_conf);
}

static void all_channels_init()
{
    for (enum channel chnl = 0; chnl < NUM_LIGHTS; chnl++) {
        channel_init(chnl);
    }
}

void white_set_power(unsigned char state)
{
    for (enum channel chnl = MAIN_COOL; chnl <= MAIN_WARM; chnl++) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, chnl, state);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, chnl);
    }
}

void rgbw_set_power(unsigned char state)
{
    for (enum channel chnl = AUX_W; chnl <= AUX_B; chnl++) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, chnl, state);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, chnl);
    }
}

void set_white_on_off(const esp_zb_zcl_set_attr_value_message_t *message)
{

    if(message->attribute.data.value) {
        bool on_off = *(bool *)message->attribute.data.value;
        if(on_off == false) {
            white_set_power(0);
        }
        else {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, MAIN_COOL, cool_level);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, MAIN_WARM, warm_level);

            ledc_update_duty(LEDC_LOW_SPEED_MODE, MAIN_COOL);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, MAIN_WARM);
        }
        ESP_LOGI(TAG, "White set to %s", on_off ? "On" : "Off");
    }
}

void set_rgbw_on_off(const esp_zb_zcl_set_attr_value_message_t *message)
{

    if(message->attribute.data.value) {
        bool on_off = *(bool *)message->attribute.data.value;
        if(on_off == false) {
            rgbw_set_power(0);
        }
        else {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, AUX_R, r_level);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, AUX_G, g_level);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, AUX_B, b_level);

            ledc_update_duty(LEDC_LOW_SPEED_MODE, AUX_R);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, AUX_G);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, AUX_B);
        }
        ESP_LOGI(TAG, "RGBW set to %s", on_off ? "On" : "Off");
    }
}

int lighting_init()
{
    timer_init();
    all_channels_init();
    return 0;
}
