/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <string.h>

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "esp_wifi.h"

#include "alink_platform.h"
#include "alink_product.h"
#include "alink_export.h"
#include "alink_json_parser.h"

#include "esp_alink.h"
#include "esp_alink_log.h"
#include "esp_info_store.h"
#include "esp_json_parser.h"

static const char *TAG = "alink_main";
/**
 * @brief  Clear wifi information, restart the device into the config network mode
 */
alink_err_t alink_update_router()
{
    int ret = 0;
    ALINK_LOGI("clear wifi config");
    ret = esp_info_erase(NVS_KEY_WIFI_CONFIG);

    if (ret != ALINK_OK) {
        ALINK_LOGD("esp_info_erase, ret: %d", ret);
    }

    ALINK_LOGW("The system is about to be restarted");
    esp_restart();

    return ALINK_OK;
}

static alink_err_t alink_connect_ap()
{
    alink_err_t ret = ALINK_ERR;
    wifi_config_t wifi_config;

    ret = esp_info_load(NVS_KEY_WIFI_CONFIG, &wifi_config, sizeof(wifi_config_t));

    if (ret > 0) {
        if (platform_awss_connect_ap(WIFI_WAIT_TIME, (char *)wifi_config.sta.ssid, (char *)wifi_config.sta.password,
                                     0, 0, wifi_config.sta.bssid, 0) == ALINK_OK) {
            return ALINK_OK;
        }
    }

    alink_event_send(ALINK_EVENT_CONFIG_NETWORK);
    ALINK_LOGI("*********************************");
    ALINK_LOGI("*    ENTER SAMARTCONFIG MODE    *");
    ALINK_LOGI("*********************************");
    ret = awss_start();

    if (ret != ALINK_OK) {
        ALINK_LOGW("awss_start is err");
        ALINK_LOGW("The system is about to be restarted");
        esp_restart();
    }

    return ALINK_OK;
}

/**
 * @brief Clear all the information of the device and return to the factory status
 */
alink_err_t alink_factory_setting()
{
    /* clear ota data  */
    ALINK_LOGI("*********************************");
    ALINK_LOGI("*          FACTORY RESET        *");
    ALINK_LOGI("*********************************");
    ALINK_LOGI("clear wifi config");
    alink_err_t err;
    err = esp_info_erase(ALINK_SPACE_NAME);
    ALINK_ERROR_CHECK(err != 0, ALINK_ERR, "alink_erase_wifi_config");

    esp_partition_t find_partition;
    memset(&find_partition, 0, sizeof(esp_partition_t));
    find_partition.type = ESP_PARTITION_TYPE_DATA;
    find_partition.subtype = ESP_PARTITION_SUBTYPE_DATA_OTA;

    const esp_partition_t *partition = esp_partition_find_first(find_partition.type, find_partition.subtype, NULL);
    ALINK_ERROR_CHECK(partition == NULL, ALINK_ERR, "nvs_erase_key partition:%p", partition);

    err = esp_partition_erase_range(partition, 0, partition->size);
    ALINK_ERROR_CHECK(partition == NULL, ALINK_ERR, "esp_partition_erase_range ret:%d", err);

    if (err != ALINK_OK) {
        ALINK_LOGE("esp_partition_erase_range ret:%d", err);
        vTaskDelete(NULL);
    }

    ALINK_LOGI("reset user account binding");
    alink_factory_reset();

    ALINK_LOGW("The system is about to be restarted");
    esp_restart();
}

/**
 * @brief Get time from alink service
 */
#define TIME_STR_LEN    (32)
alink_err_t alink_get_time(unsigned int *utc_time)
{
    ALINK_PARAM_CHECK(utc_time);

    char buf[TIME_STR_LEN] = { 0 }, *attr_str;
    int size = TIME_STR_LEN, attr_len = 0;

    if (!alink_query("getAlinkTime", "{}", buf, &size)) {
        return ALINK_ERR;
    }

    attr_str = json_get_value_by_name(buf, size, "time", &attr_len, NULL);
    ALINK_ERROR_CHECK(!attr_str, ALINK_ERR, "json_get_value_by_name, ret: %p", attr_str);

    *utc_time = atoi(attr_str);

    return ALINK_OK;
}

/**
 * @brief Event management
 */
#define EVENT_QUEUE_NUM         3
static xQueueHandle xQueueEvent = NULL;
static void alink_event_loop_task(void *pvParameters)
{
    alink_event_cb_t s_event_handler_cb = (alink_event_cb_t)pvParameters;

    for (;;) {
        alink_event_t event;

        if (xQueueReceive(xQueueEvent, &event, portMAX_DELAY) != pdPASS) {
            continue;
        }

        if (!s_event_handler_cb) {
            continue;
        }

        if ((*s_event_handler_cb)(event) < 0) {
            ALINK_LOGW("Event handling failed");
        }
    }

    vTaskDelete(NULL);
}

alink_err_t alink_event_send(alink_event_t event)
{
    if (!xQueueEvent) {
        xQueueEvent = xQueueCreate(EVENT_QUEUE_NUM, sizeof(alink_event_t));
    }

    alink_err_t ret = xQueueSend(xQueueEvent, &event, 0);
    ALINK_ERROR_CHECK(ret != pdTRUE, ALINK_ERR, "xQueueSendToBack fail!");

    return ALINK_OK;
}

/**
 * @brief Initialize alink config and start alink task
 */
extern alink_err_t alink_trans_init();
extern void alink_trans_destroy();
alink_err_t alink_init(_IN_ const void *product_info,
                       _IN_ const alink_event_cb_t event_handler_cb)
{
    ALINK_PARAM_CHECK(product_info);
    ALINK_PARAM_CHECK(event_handler_cb);

    alink_err_t ret = ALINK_OK;

    if (!xQueueEvent) {
        xQueueEvent = xQueueCreate(EVENT_QUEUE_NUM, sizeof(alink_event_t));
    }

    xTaskCreate(alink_event_loop_task, "alink_event_loop_task", ALINK_EVENT_STACK_SIZE,
                event_handler_cb, DEFAULU_TASK_PRIOTY, NULL);

    ret = product_set(product_info);
    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "product_set :%d", ret);

    ret = alink_connect_ap();
    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "alink_connect_ap :%d", ret);

    ret = alink_trans_init();

    if (ret != ALINK_OK) {
        alink_trans_destroy();
    }

    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "alink_trans_init :%d", ret);

    return ALINK_OK;
}
