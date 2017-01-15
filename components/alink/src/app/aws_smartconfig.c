#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_system.h"

#include "string.h"
#include "platform.h"
#include "product.h"
#include "alink_user_config.h"
static const char *TAG = "alink_smartconfig";

alink_err_t aws_smartconfig_init(_OUT_ wifi_config_t *wifi_config)
{
    char bssid[6];
    char auth;
    char encry;
    char channel;

    char product_model[PRODUCT_MODEL_LEN];
    char product_secret[PRODUCT_SECRET_LEN];
    char device_mac[PLATFORM_MAC_LEN];

    product_get_secret(product_secret);
    product_get_model(product_model);
    platform_wifi_get_mac(device_mac);

    aws_start(product_model, product_secret, device_mac, NULL);
    alink_err_t ret = aws_get_ssid_passwd((char *)wifi_config->sta.ssid, (char *)wifi_config->sta.password,
                              &bssid[0], &auth, &encry, &channel);

    ALINK_ERROR_CHECK(ret != pdTRUE, ALINK_ERR, "alink wireless setup timeout, ret:%x", ret);

    return ALINK_OK;
}
