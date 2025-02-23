#include "freertos/FreeRTOS.h"

/* Espressif */
#include "esp_check.h"
#include "nvs_flash.h"
#include "ha/esp_zigbee_ha_standard.h"

/* Local */
#include "lighting.h"
#include "endpoint.h"

static const char *TAG = "ZB_DUAL_LIGHT";

/* Zigbee configuration */
#define MAX_CHILDREN                      10                                    /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE         false                                 /* enable the install code policy for security */
#define ESP_ZB_PRIMARY_CHANNEL_MASK       ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK  /* Zigbee primary channel mask use in the example */


// nested cases are a bit messier than I thought...
static void white_endpoint_attr_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    switch (message->info.cluster) {
  
    /* OnOff cluster attributes */
    case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:
        switch (message->attribute.id) {
        case ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID:
            set_white_on_off(message);
            break;
        default:
            ESP_LOGW(TAG, "On/Off cluster data: attribute(0x%x), type(0x%x)", message->attribute.id, message->attribute.data.type);
            break;
        }
        break;

    /* Level control attributes */
    case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
        switch (message->attribute.id) {
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID:
            set_white_level(message);
            break;
        default:
            ESP_LOGW(TAG, "Level cluster data: attribute(0x%x), type(0x%x)", message->attribute.id, message->attribute.data.type);
            break;
        }
        break;

    /* Color attributes (todo) */
    case ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
        switch (message->attribute.id) {
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID:
            set_white_temp(message);
            break;
        default:
            ESP_LOGW(TAG, "Color cluster data: attribute(0x%x), type(0x%x)", message->attribute.id, message->attribute.data.type);
                break;
            }
            break;

    default:
        ESP_LOGI(TAG, "Message data: cluster(0x%x), attribute(0x%x)  ", message->info.cluster, message->attribute.id);
        break;
    }
}

static void rgbw_endpoint_attr_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    switch (message->info.cluster) {
  
    /* OnOff cluster attributes */
    case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:
        switch (message->attribute.id) {
        case ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID:
            set_rgbw_on_off(message);
            break;
        default:
            ESP_LOGW(TAG, "On/Off cluster data: attribute(0x%x), type(0x%x)", message->attribute.id, message->attribute.data.type);
            break;
        }
        break;

    /* Level control attributes */
    case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
        switch (message->attribute.id) {
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID:
                set_rgbw_level(message);
                break;
        default:
            ESP_LOGW(TAG, "Level cluster data: attribute(0x%x), type(0x%x)", message->attribute.id, message->attribute.data.type);
            break;
        }
        break;
    
    /* Color attributes */
    case ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
        switch (message->attribute.id) {
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID:
            set_rgbw_x(message);
            break;
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID:
            set_rgbw_y(message);
            break;
        default:
            ESP_LOGW(TAG, "Color cluster data: attribute(0x%x), type(0x%x)", message->attribute.id, message->attribute.data.type);
                break;
            }
            break;

    default:
        ESP_LOGI(TAG, "Message data: cluster(0x%x), attribute(0x%x)  ", message->info.cluster, message->attribute.id);
        break;
    }
}

/* Attribute change handler, whatever that means. Gets messages for specific endpoints. */
// The default esp dimmable light structure gives us Basic, Identify, Groups, Scenes, OnOff (?)
// Ikea light has Basic, Color, Diagnostic, GreenPowerProxy, Groups, Identify, LevelControl, LightLink, ManufacturerSpecific, OnOff, Ota, Scenes
static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);

    switch (message->info.dst_endpoint) {
        case WHITE_ENDPOINT:
            white_endpoint_attr_handler(message);
            break;
        case RGBW_ENDPOINT:
            rgbw_endpoint_attr_handler(message);
            break;
    }
    
    return ret;
}

/* Main callback for network messages: can add in ota stuff and more here */
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee commissioning");
}

/* Callback from zb stack for post-init handlers and some protocol stuff */
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Deferred driver initialization %s", lighting_init() ? "failed" : "successful");
            ESP_LOGI(TAG, "Device started up in %sfactory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non ");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
            }
        } else {
            ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        if (err_status == ESP_OK) {
            if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
                ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
            } else {
                ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
            }
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
        break;
    }
}

/* Add attributes to basic cluster */
static esp_err_t basic_cluster_init_attributes(esp_zb_ep_list_t *ep_list, uint8_t endpoint_id)
{
    esp_err_t ret = ESP_OK;
    esp_zb_cluster_list_t *cluster_list = NULL;
    esp_zb_attribute_list_t *basic_cluster = NULL;

    cluster_list = esp_zb_ep_list_get_ep(ep_list, endpoint_id);
    ESP_RETURN_ON_FALSE(cluster_list, ESP_ERR_INVALID_ARG, TAG, "Failed to find endpoint id: %d in list: %p", endpoint_id, ep_list);
    basic_cluster = esp_zb_cluster_list_get_cluster(cluster_list, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    ESP_RETURN_ON_FALSE(basic_cluster, ESP_ERR_INVALID_ARG, TAG, "Failed to find basic cluster in endpoint: %d", endpoint_id);
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ESP_MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ESP_MODEL_IDENTIFIER));
    return ret;
}

/* Main task: zigbee protocol init, cluster init, endpoint init, then give control to zb stack */
static void esp_zb_task(void *pvParameters)
{
    /* Some stack init */
    esp_zb_cfg_t zb_nwk_cfg = 
        {
            .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,
            .install_code_policy = INSTALLCODE_POLICY_ENABLE,
            .nwk_cfg.zczr_cfg = {
                .max_children = MAX_CHILDREN,
            }
        };
    esp_zb_init(&zb_nwk_cfg);
    
    esp_zb_ep_list_t* endpoint_list = esp_zb_ep_list_create();

    /* White light endpoint init */
    esp_zb_cluster_list_t *white_cluster_list = esp_zb_zcl_cluster_list_create();

    // The boilerplate clusters_create function here doesn't work for color temperature lights. Gotta make it from scratch.
    // Ikea light has Basic, Color, Diagnostic, GreenPowerProxy, Groups, Identify, LevelControl, LightLink, ManufacturerSpecificCluster(0xfc7c), OnOff, Ota, Scenes
    // Zigbee lighting and occupancy says we *must* have Basic, Identify, Groups, Scenes, On/Off, Level control, and Color control. So this is *broken*! Hopefully HA doesn't care that much.
    // Also, all of these, and LOTS of other stuff throughout this code reserves the right to throw errors and we just ignore that. Im okay with that on my dumb little experimental light, though.
    esp_zb_attribute_list_t* attrs;
    attrs = esp_zb_basic_cluster_create(&white_basic_cfg);
    esp_zb_cluster_list_add_basic_cluster(white_cluster_list, attrs, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    attrs = esp_zb_on_off_cluster_create(&white_on_off_cfg);
    esp_zb_cluster_list_add_on_off_cluster(white_cluster_list, attrs, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    attrs = esp_zb_level_cluster_create(&white_level_cfg);
    esp_zb_cluster_list_add_level_cluster(white_cluster_list, attrs, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // The docs arent explicit with what the lifetime of these needs to be. I've seen supposedly working examples where they're thrown out after the call to add_attr
    // but the fact that it takes a pointer worries me. I think the pointer is just because it needs to be opaque.
    uint16_t color_attr = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_DEF_VALUE;
    uint16_t min_temp = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_MIREDS_DEFAULT_VALUE;
    uint16_t max_temp = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_MIREDS_DEFAULT_VALUE;
    uint8_t color_mode = COLOR_MODE_MIREDS;
    uint8_t enhanced_color_mode = COLOR_MODE_MIREDS;
    uint8_t color_capabilities = COLORTEMP_SUPPORTED;

    /* Pretty sure these arent all of the attrs required by spec, so once again *broken* */
    attrs = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL);
    esp_zb_color_control_cluster_add_attr(attrs, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID, &color_mode);
    esp_zb_color_control_cluster_add_attr(attrs, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_ENHANCED_COLOR_MODE_ID, &enhanced_color_mode);
    esp_zb_color_control_cluster_add_attr(attrs, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_CAPABILITIES_ID, &color_capabilities);
    esp_zb_color_control_cluster_add_attr(attrs, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID, &color_attr);
    esp_zb_color_control_cluster_add_attr(attrs, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_MIREDS_ID, &min_temp);
    esp_zb_color_control_cluster_add_attr(attrs, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_MIREDS_ID, &max_temp);
    esp_zb_cluster_list_add_color_control_cluster(white_cluster_list, attrs, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_zb_ep_list_add_ep(endpoint_list, white_cluster_list, white_endpoint_cfg);

    /* RGBW light endpoint init */
    // The boilerplate cluster layout and config works for the rgbw light
    esp_zb_cluster_list_t* rgbw_cluster_list = esp_zb_color_dimmable_light_clusters_create(&rgbw_cluster_cfg);
    esp_zb_ep_list_add_ep(endpoint_list, rgbw_cluster_list, rgbw_endpoint_cfg);

    /* Add manufacturer info to the 'basic' cluster of these endpoints */
    basic_cluster_init_attributes(endpoint_list, WHITE_ENDPOINT);
    basic_cluster_init_attributes(endpoint_list, RGBW_ENDPOINT);

    /* Register all endpoints. */
    esp_zb_device_register(endpoint_list);

    /* Action handler for actually handling certain incoming messages? What determines which things are static and which are dynamic through this handler? */
    esp_zb_core_action_handler_register(zb_action_handler);

    /* Finish off the stack init */
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

/* Entry point: basic hardware init */
void app_main(void)
{
    esp_zb_platform_config_t config = {
        .radio_config = { .radio_mode = ZB_RADIO_MODE_NATIVE },
        .host_config  = { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE },
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
