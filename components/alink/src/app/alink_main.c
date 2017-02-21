/******************************************************************************
 * Copyright (C) 2014 -2016  Espressif System
 *
 * FileName: app_main.c
 *
 * Description:
 *
 * Modification history:
 * 2016/11/16, v0.0.1 create this file.
*******************************************************************************/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "string.h"
#include "platform.h"
#include "product.h"
#include "esp_partition.h"
#include "esp_alink.h"
static const char *TAG = "alink_main";

extern void alink_trans_init();
extern void alink_key_init(uint32_t key_gpio_pin);
extern alink_err_t alink_key_scan(TickType_t ticks_to_wait);

alink_err_t alink_read_wifi_config(_OUT_ wifi_config_t *wifi_config)
{
    ALINK_PARAM_CHECK(wifi_config == NULL);
    alink_err_t ret   = -1;
    nvs_handle handle = 0;
    size_t length = sizeof(wifi_config_t);
    memset(wifi_config, 0, length);

    ret = nvs_open("ALINK", NVS_READWRITE, &handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_open ret:%x", ret);
    ret = nvs_get_blob(handle, "wifi_config", wifi_config, &length);
    nvs_close(handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ALINK_LOGD("nvs_get_blob ret:%x,No data storage,the read data is empty", ret);
        return ALINK_ERR;
    }
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_get_blob ret:%x", ret);
    return ALINK_OK;
}

alink_err_t alink_write_wifi_config(_IN_ const wifi_config_t *wifi_config)
{
    ALINK_PARAM_CHECK(wifi_config == NULL);
    alink_err_t ret   = -1;
    nvs_handle handle = 0;

    ret = nvs_open("ALINK", NVS_READWRITE, &handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_open ret:%x", ret);

    ret = nvs_set_blob(handle, "wifi_config", wifi_config, sizeof(wifi_config_t));
    nvs_commit(handle);
    nvs_close(handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_set_blob ret:%x", ret);
    return ALINK_OK;
}

alink_err_t alink_erase_wifi_config()
{
    alink_err_t ret   = -1;
    nvs_handle handle = 0;

    ret = nvs_open("ALINK", NVS_READWRITE, &handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_open ret:%x", ret);
    ret = nvs_erase_key(handle, "wifi_config");
    nvs_commit(handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_erase_key ret:%x", ret);
    return ALINK_OK;
}

alink_err_t alink_erase_all_config()
{
    alink_err_t ret     = -1;
    nvs_handle handle = 0;

    ret = nvs_open("ALINK", NVS_READWRITE, &handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_open ret:%x", ret);
    ret = nvs_erase_all(handle);
    nvs_commit(handle);
    nvs_close(handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_erase_key ret:%x", ret);
    return ALINK_OK;
}

void factory_reset(void* arg)
{    alink_err_t ret = alink_key_scan(portMAX_DELAY);
    ALINK_ERROR_CHECK(ret != ALINK_OK, vTaskDelete(NULL), "alink_key_scan ret:%d", ret);
    /* clear ota data  */
    alink_err_t err;
    esp_partition_t find_partition;
    memset(&find_partition, 0, sizeof(esp_partition_t));
    find_partition.type = ESP_PARTITION_TYPE_DATA;
    find_partition.subtype = ESP_PARTITION_SUBTYPE_DATA_OTA;

    const esp_partition_t *partition = esp_partition_find_first(find_partition.type, find_partition.subtype, NULL);
    if (partition == NULL) {
        ALINK_LOGE("nvs_erase_key partition:%p", partition);
        vTaskDelete(NULL);
    }

    err = esp_partition_erase_range(partition, 0, partition->size);
    if (err != ALINK_OK) {
        ALINK_LOGE("esp_partition_erase_range ret:%d", err);
        vTaskDelete(NULL);
    }

    alink_erase_wifi_config();
    // alink_erase_all_config();
    ALINK_LOGI("reset user account binding");
    alink_factory_reset();
    ALINK_LOGI("factory_reset is finsh, The system is about to be restarted");
    esp_restart();

    vTaskDelete(NULL);
}

alink_err_t alink_connect_ap()
{
    alink_err_t ret = ALINK_ERR;
    wifi_config_t wifi_config;

    ret = alink_read_wifi_config(&wifi_config);
    if (ret == ALINK_OK) {
        if (platform_awss_connect_ap(WIFI_WAIT_TIME, (char *)wifi_config.sta.ssid, (char *)wifi_config.sta.password,
                                     0, 0, wifi_config.sta.bssid, 0) == ALINK_OK) {
            return ALINK_OK;
        }
    }

    ALINK_LOGI("*********************************");
    ALINK_LOGI("*    ENTER SAMARTCONFIG MODE    *");
    ALINK_LOGI("*********************************");
    ret = awss_start();
    awss_stop();
    if (ret != ALINK_OK) {
        ALINK_LOGI("awss_start is err ret: %d", ret);
        esp_restart();
    }
    return ALINK_OK;
}

/******************************************************************************
 * FunctionName : esp_alink_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void esp_alink_init(_IN_ const void *product_info)
{
    alink_key_init(ALINK_RESET_KEY_IO);
    xTaskCreate(factory_reset, "factory_reset", 1024 * 4, NULL, 10, NULL);
    product_set(product_info);

    alink_connect_ap();
    alink_trans_init(NULL);}
