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

#include "nvs.h"
#include "nvs_flash.h"
#include "string.h"
#include "platform.h"
#include "product.h"
#include "aws_smartconfig.h"
#include "aws_softap.h"

#define WIFI_WAIT_TIME      (60 * 1000 / portTICK_RATE_MS)
static SemaphoreHandle_t xSemConnet = NULL;
void alink_passthroug(void *arg);

BaseType_t alink_read_wifi_config(wifi_config_t *wifi_config)
{
    BaseType_t ret     = -1;
    nvs_handle handle = 0;
    size_t length = sizeof(wifi_config_t);
    if (wifi_config == NULL) return pdFALSE;

    ret = nvs_open("ALINK", NVS_READWRITE, &handle);
    if (ret != 0) return pdFALSE;
    ret = nvs_get_blob(handle, "wifi_config", wifi_config, &length);
    nvs_close(handle);
    return (ret) ? pdFALSE : pdTRUE;
}

BaseType_t alink_write_wifi_config(wifi_config_t *wifi_config)
{
    BaseType_t ret     = -1;
    nvs_handle handle = 0;
    if (wifi_config == NULL) return pdFALSE;

    ret = nvs_open("ALINK", NVS_READWRITE, &handle);
    if (ret != 0) return pdFALSE;
    ret = nvs_set_blob(handle, "wifi_config", wifi_config, sizeof(wifi_config_t));
    nvs_commit(handle);
    nvs_close(handle);
    return (ret) ? pdFALSE : pdTRUE;
}


BaseType_t alink_erase_wifi_config()
{
    esp_err_t ret     = -1;
    nvs_handle handle = 0;

    ret = nvs_open("ALINK", NVS_READWRITE, &handle);
    if (ret != 0) return pdFALSE;
    ret = nvs_erase_key(handle, "wifi_config");
    nvs_commit(handle);
    nvs_close(handle);
    return (ret) ? pdFALSE : pdTRUE;
}


static esp_err_t event_handler(void *ctx, system_event_t *event)
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
    return ESP_OK;
}

static BaseType_t wifi_sta_connect_ap(wifi_config_t *wifi_config, TickType_t ticks_to_wait)
{
    printf("WiFi SSID: %s, password: %s\n", wifi_config->ap.ssid, wifi_config->ap.password);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    if (xSemConnet == NULL)
        xSemConnet = xSemaphoreCreateBinary();
    return xSemaphoreTake(xSemConnet, ticks_to_wait);
}


BaseType_t alink_connect_ap()
{
    BaseType_t ret = pdFALSE;

    wifi_config_t wifi_config;
    esp_event_loop_set_cb(event_handler, NULL);
    xSemConnet = xSemaphoreCreateBinary();

    ret = alink_read_wifi_config(&wifi_config);
    if (ret == pdTRUE) {
        if (wifi_sta_connect_ap(&wifi_config, WIFI_WAIT_TIME) == pdTRUE)
            return pdTRUE;
    }
    esp_wifi_get_config(WIFI_IF_STA, &wifi_config);

    ret = aws_smartconfig_init(&wifi_config);
    if (ret == pdTRUE) {
        if (wifi_sta_connect_ap(&wifi_config, WIFI_WAIT_TIME) == pdTRUE)
            goto EXIT;
    }

    printf("********ENTER SOFTAP MODE******\n");
    ret = aws_softap_init(&wifi_config);
    if (ret == pdTRUE) {
        if (wifi_sta_connect_ap(&wifi_config, WIFI_WAIT_TIME) == pdTRUE)
            goto EXIT;
    }

    return pdFALSE;
EXIT:
    alink_write_wifi_config(&wifi_config);
    aws_notify_app();
    aws_destroy();
    return pdTRUE;
}


/******************************************************************************
 * FunctionName : esp_alink_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void esp_alink_init()
{
    alink_connect_ap();
    xTaskCreate(alink_passthroug, "alink_passthroug", 4096, NULL, 4, NULL);
}
