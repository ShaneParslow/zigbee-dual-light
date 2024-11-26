#include "driver/ledc.h"

#include "lighting.h"

const static gpio_num_t pin_map[NUM_LIGHTS] = {
    [MAIN_COOL] = GPIO_NUM_3,
    [MAIN_WARM] = GPIO_NUM_11,
    [AUX_W]     = GPIO_NUM_1,
    [AUX_R]     = GPIO_NUM_7,
    [AUX_G]     = GPIO_NUM_5,
    [AUX_B]     = GPIO_NUM_4,
};

static void timer_init()
{
    ledc_timer_config_t pwm_conf = {};
	
	pwm_conf.speed_mode = LEDC_LOW_SPEED_MODE;
	pwm_conf.timer_num = 0;
	pwm_conf.freq_hz = 1250000;
	pwm_conf.duty_resolution = LEDC_TIMER_6_BIT; // TODO: lower frequency, higher res?
	pwm_conf.clk_cfg = LEDC_USE_PLL_DIV_CLK;

    ledc_timer_config(&pwm_conf);
}

static void channel_init(enum channel chnl)
{
    ledc_channel_config_t chnl_conf = {};

	chnl_conf.gpio_num = pin_map[chnl];
	chnl_conf.channel = chnl;
	chnl_conf.timer_sel = 0;
	chnl_conf.duty = 32; //todo

	ledc_channel_config(&chnl_conf);
}

static void all_channels_init()
{
    for (enum channel chnl = 0; chnl < NUM_LIGHTS; chnl++) {
        channel_init(chnl);
    }
}

void white_light_set_power(bool state)
{
    for (enum channel chnl = MAIN_COOL; chnl <= MAIN_WARM; chnl++) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, chnl, 63 * state);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, chnl);
    }
}

void rgbw_light_set_power(bool state)
{
    for (enum channel chnl = AUX_W; chnl < AUX_B; chnl++) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, chnl, 63 * state);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, chnl);
    }
}

int lighting_init()
{
    timer_init();
    all_channels_init();
    return 0;
}
