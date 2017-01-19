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
#include "aws_smartconfig.h"
#include "aws_softap.h"
#include "esp_partition.h"
#include "alink_user_config.h"
static const char *TAG = "alink_main";

static SemaphoreHandle_t xSemConnet = NULL;
static SemaphoreHandle_t xSemAinkInitFinsh = NULL;
void alink_passthroug(void *arg);
void alink_json(void *arg);
void alink_key_init(uint32_t key_gpio_pin);
alink_err_t alink_key_scan(TickType_t ticks_to_wait);

alink_err_t alink_read_wifi_config(_OUT_ wifi_config_t *wifi_config)
{
    ALINK_PARAM_CHECK(wifi_config == NULL);
    alink_err_t ret     = -1;
    nvs_handle handle = 0;
    size_t length = sizeof(wifi_config_t);
    memset(wifi_config, 0, length);

    ret = nvs_open("ALINK", NVS_READWRITE, &handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_open ret:%x", ret);
    ret = nvs_get_blob(handle, "wifi_config", wifi_config, &length);
    nvs_close(handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ALINK_LOGD("[%s, %d]:nvs_get_blob ret:%x,No data storage,the read data is empty\n",
                   __func__, __LINE__ , ret);
        return ALINK_ERR;
    }
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_get_blob ret:%x", ret);
    return ALINK_OK;
}

alink_err_t alink_write_wifi_config(_IN_ const wifi_config_t *wifi_config)
{
    ALINK_PARAM_CHECK(wifi_config == NULL);
    alink_err_t ret     = -1;
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
    alink_err_t ret     = -1;
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
{
    if (xSemAinkInitFinsh == NULL) xSemAinkInitFinsh = xSemaphoreCreateBinary();
    alink_err_t ret = alink_key_scan(portMAX_DELAY);
    ALINK_ERROR_CHECK(ret != ALINK_OK, vTaskDelete(NULL), "alink_key_scan ret:%d", ret);
    /* clear ota data  */
    alink_err_t err;
    esp_partition_t find_partition;
    memset(&find_partition, 0, sizeof(esp_partition_t));
    find_partition.type = ESP_PARTITION_TYPE_DATA;
    find_partition.subtype = ESP_PARTITION_SUBTYPE_DATA_OTA;

    const esp_partition_t *partition = esp_partition_find_first(find_partition.type, find_partition.subtype, NULL);
    ALINK_ERROR_CHECK(partition == NULL, ; , "nvs_erase_key ret");
    err = esp_partition_erase_range(partition, 0, partition->size);
    ALINK_ERROR_CHECK(err != ESP_OK, ; , "esp_partition_erase_range ret:%x", err);

    if (err != ALINK_OK) {
        ALINK_LOGE("esp_partition_erase_range failed! err=0x%x", err);
        vTaskDelete(NULL);
    }

    if (xSemaphoreTake(xSemAinkInitFinsh, 0) != pdTRUE) {
        ALINK_LOGW("alink_os is not initialized, Unable to unbundle");
    } else {
        alink_factory_reset();
    }
    alink_erase_all_config();
    // alink_erase_wifi_config();

    ALINK_LOGI("factory_reset is finsh, The system is about to be restarted");
    esp_restart();

    vTaskDelete(NULL);
}

static alink_err_t event_handler(void *ctx, system_event_t *event)
{
    if (xSemConnet == NULL)
        xSemConnet = xSemaphoreCreateBinary();
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xSemaphoreGive(xSemConnet);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        break;
    default:
        break;
    }
    return ALINK_OK;
}

static alink_err_t wifi_sta_connect_ap(wifi_config_t *wifi_config, TickType_t ticks_to_wait)
{
    ALINK_PARAM_CHECK(wifi_config == NULL);
    printf("WiFi SSID: %s, password: %s\n", wifi_config->ap.ssid, wifi_config->ap.password);

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    BaseType_t err = xSemaphoreTake(xSemConnet, ticks_to_wait);
    ALINK_ERROR_CHECK(err != pdTRUE, ALINK_ERR, "xSemaphoreTake ret:%x wait: %d", err, ticks_to_wait);
    return ALINK_OK;
}

alink_err_t alink_connect_ap()
{
    alink_err_t ret = ALINK_ERR;
    wifi_config_t wifi_config;
    esp_event_loop_set_cb(event_handler, NULL);
    xSemConnet = xSemaphoreCreateBinary();

    ret = alink_read_wifi_config(&wifi_config);
    if (ret == ALINK_OK) {
        if (wifi_sta_connect_ap(&wifi_config, WIFI_WAIT_TIME) == ALINK_OK)
            return ALINK_OK;
    }
    esp_wifi_get_config(WIFI_IF_STA, &wifi_config);

    ret = aws_smartconfig_init(&wifi_config);
    if (ret == ALINK_OK) {
        if (wifi_sta_connect_ap(&wifi_config, WIFI_WAIT_TIME) == ALINK_OK)
            goto EXIT;
    }

    printf("********ENTER SOFTAP MODE******\n");
    ret = aws_softap_init(&wifi_config);
    if (ret == ALINK_OK) {
        if (wifi_sta_connect_ap(&wifi_config, WIFI_WAIT_TIME) == ALINK_OK)
            goto EXIT;
    }

    return ALINK_ERR;
EXIT:

    alink_write_wifi_config(&wifi_config);
    aws_notify_app();
    aws_destroy();
    return ALINK_OK;
}

/******************************************************************************
 * FunctionName : esp_alink_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void esp_alink_init(_IN_ const struct device_info *product_info)
{
    alink_key_init(ALINK_RESET_KEY_IO);
    xTaskCreate(factory_reset, "factory_reset", 1024 * 4, NULL, 10, NULL);

    product_set(product_info);

    alink_connect_ap();
    xTaskCreate(alink_json, "alink_json", 1024 * 4, NULL, 9, NULL);
    // xTaskCreate(alink_passthroug, "alink_passthroug", 1024 * 4, NULL, 9, NULL);
}
