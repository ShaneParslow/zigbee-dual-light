#include "esp_check.h"
#include "esp_zigbee_core.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"

esp_err_t zb_ota_upgrade_query_image_resp_handler(esp_zb_zcl_ota_upgrade_query_image_resp_message_t message);
esp_err_t zb_ota_upgrade_status_handler(esp_zb_zcl_ota_upgrade_value_message_t message);
