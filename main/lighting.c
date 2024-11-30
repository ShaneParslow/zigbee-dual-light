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

/* Save the luminance/level of the lights. The color/temp needs to be scaled by this, which is a todo */
static uint8_t white_level  = 0xfe;
static uint8_t rgbw_level   = 0xfe;

/* Saved chromaticity */
static uint16_t white_temp = 0;
static uint16_t rgbw_x = ESP_ZB_ZCL_COLOR_CONTROL_CURRENT_X_DEF_VALUE;
static uint16_t rgbw_y = ESP_ZB_ZCL_COLOR_CONTROL_CURRENT_Y_DEF_VALUE;

/* Save levels, even when the light gets turned off so that we can restore it.
    Technically, we are supposed to be using the StartupOnOff attribute but I
    don't think home assistant is going to care if this implementation is
    broken, based on what messages it's been sending. I havent seen anything
    addressed to the StartupOnOff attribute. Still, broken implementation. */
/* todo: remove in favor of saving levels and x/y/temp. From that, these can be calculated */
static unsigned char cool_level = 0xfe;
static unsigned char warm_level = 0xfe;

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

void white_set_power(uint8_t state)
{
    for (enum channel chnl = MAIN_COOL; chnl <= MAIN_WARM; chnl++) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, chnl, state);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, chnl);
    }
}

void rgbw_set_power(uint8_t state)
{
    for (enum channel chnl = AUX_W; chnl <= AUX_B; chnl++) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, chnl, state);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, chnl);
    }
}

void set_white_on_off(const esp_zb_zcl_set_attr_value_message_t *message)
{

    if(message->attribute.data.value && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
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
    else {
        ESP_LOGI(TAG, "Invalid OnOff data and/or type");
    }
}

static void update_rgbw()
{
    // Floats are actually required because the xyY to XYZ has some terms that get quite close to 1 and I don't want to add in magic coefficients for fixed point arithmetic
    // Also the equation for xyY to XYZ is for normalized variables, but I'm pretty sure I could get past that by switching out the 1 with a UINT16_MAX.
    double x = (double)rgbw_x / (double)UINT16_MAX;
    double y = (double)rgbw_y / (double)UINT16_MAX;
    double Y = (double)rgbw_level / (double)UINT8_MAX;
    double X, Z;
    double r, g, b, w;
    uint8_t r_final, g_final, b_final, w_final;

    // These equations blow up at y = 0;
    if (rgbw_y == 0) {
        w = r = g = b = 0;
        goto update_leds;
    }

    // okay, so we are working with cie xyY color space. that means we have chromaticity with x and y, and luminance with Y.
    // xy defines the ratio of r/g/b. Y defines the intensity of the light, and is taken directly from the level setting.
    // Y can directly set W, and should probably also scale r/g/b
    // in order to make sure that the colors aren't washed out by the white, we should probably max r/g/b at 50% and then start fading in the white.

    /* Start fading W in at half luminance */
    //int w = (rgbw_level - 127) * 2;
    //w_final = w > 0 ? w : 0;
    w = 0; // temp, TODO: limit luminance value for rgb calculations so that it doesn't start getting washed out. also should help save on power budget.
    // wait. we can just make r/g/b not depend on Y (luminance) in the matrix and do luminance *solely* with w. that should help a lot with the power budget. gotta test.

    /* Conversion from xyY to XYZ. Mind the capital vs lowercase y's */
    /* Capital is luminance in both xyY and XYZ. Lower case is part of chromaticity in xyY */
    // https://en.wikipedia.org/wiki/CIE_1931_color_space#CIE_xy_chromaticity_diagram_and_the_CIE_xyY_color_space
    X = (x * Y) / y;
    Z = (Y / y) * (1 - x - y);

    /* XYZ to srgb matrix transform */
    // https://en.wikipedia.org/wiki/CIE_1931_color_space#Construction_of_the_CIE_XYZ_color_space_from_the_Wright%E2%80%93Guild_data
    // https://www.oceanopticsbook.info/view/photometry-and-visibility/from-xyz-to-rgb
    r = ((X * M11) + (Y * M12) + (Z * M13));
    g = ((X * M21) + (Y * M22) + (Z * M23));
    b = ((X * M31) + (Y * M32) + (Z * M33));

    ESP_LOGI(TAG, "xy: %f %f  XYZ: %f %f %f", x, y, X, Y, Z);

update_leds:
    w_final = w < 1 ? round(w * 255.0) : 255;
    r_final = r < 1 ? round(w * 255.0) : 255;
    g_final = g < 1 ? round(w * 255.0) : 255;
    b_final = b < 1 ? round(w * 255.0) : 255;
    
    ESP_LOGI(TAG, "RGBW: %hhu %hhu %hhu %hhu", r_final, g_final, b_final, w_final);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, AUX_W, w_final);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, AUX_R, r_final);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, AUX_G, g_final);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, AUX_B, b_final);

    ledc_update_duty(LEDC_LOW_SPEED_MODE, AUX_W);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, AUX_R);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, AUX_G);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, AUX_B);
}

void set_rgbw_on_off(const esp_zb_zcl_set_attr_value_message_t *message)
{

    if(message->attribute.data.value && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
        bool on_off = *(bool *)message->attribute.data.value;
        if(on_off == false) {
            rgbw_set_power(0);
        }
        else {
            update_rgbw();
        }
        ESP_LOGI(TAG, "RGBW set to %s", on_off ? "On" : "Off");
    }
    else {
        ESP_LOGI(TAG, "Invalid OnOff data and/or type");
    }
}

void set_rgbw_level(const esp_zb_zcl_set_attr_value_message_t *message)
{
    if(message->attribute.data.value && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
        rgbw_level = *(uint8_t*)message->attribute.data.value;
        update_rgbw();
    }
    else {
        ESP_LOGI(TAG, "Invalid Level data and/or type");
    }
}

void set_rgbw_x(const esp_zb_zcl_set_attr_value_message_t *message)
{
    if(message->attribute.data.value && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
        rgbw_x = *(uint16_t*)message->attribute.data.value;
        update_rgbw();
    }
    else {
        ESP_LOGI(TAG, "Invalid Color data and/or type");
    }
}

void set_rgbw_y(const esp_zb_zcl_set_attr_value_message_t *message)
{
    if(message->attribute.data.value && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
        rgbw_y = *(uint16_t*)message->attribute.data.value;
        update_rgbw();
    }
    else {
        ESP_LOGI(TAG, "Invalid Color data and/or type");
    }
}

int lighting_init()
{
    timer_init();
    all_channels_init();
    return 0;
}
