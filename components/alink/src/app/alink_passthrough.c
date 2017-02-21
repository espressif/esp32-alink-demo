
/*
 * Copyright (c) 2014-2015 Alibaba Group. All rights reserved.
 *
 * Alibaba Group retains all right, title and interest (including all
 * intellectual property rights) in and to this computer program, which is
 * protected by applicable intellectual property laws.  Unless you have
 * obtained a separate written license from Alibaba Group., you are not
 * authorized to utilize all or a part of this computer program for any
 * purpose (including reproduction, distribution, modification, and
 * compilation into object code), and you must immediately destroy or
 * return to Alibaba Group all copies of this computer program.  If you
 * are licensed by Alibaba Group, your rights to utilize this computer
 * program are limited by the terms of that license.  To obtain a license,
 * please contact Alibaba Group.
 *
 * This computer program contains trade secrets owned by Alibaba Group.
 * and, unless unauthorized by Alibaba Group in writing, you agree to
 * maintain the confidentiality of this computer program and related
 * information and to not disclose this computer program and related
 * information to any other person or entity.
 *
 * THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
 * Alibaba Group EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, TITLE, AND NONINFRINGEMENT.
 */
#include "platform/platform.h"
#include "product/product.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "esp_system.h"

#include "alink_export.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_alink.h"

#define DOWN_CMD_QUEUE_NUM  5
#define UP_CMD_QUEUE_NUM    5

#define Method_PostData    "postDeviceData"
#define Method_PostRawData "postDeviceRawData"
#define Method_GetAlinkTime "getAlinkTime"
static const char *TAG = "alink_passthrough";
extern SemaphoreHandle_t xSemWriteInfo;
static alink_err_t post_data_enable = ALINK_TRUE;

static xQueueHandle xQueueDownCmd    = NULL;
static xQueueHandle xQueueUpCmd      = NULL;
static SemaphoreHandle_t xSemWrite   = NULL;
static SemaphoreHandle_t xSemRead    = NULL;
static SemaphoreHandle_t xSemDownCmd = NULL;

static char a2x(char ch)
{
    switch (ch) {
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
        return 9;
    case 'A':
    case 'a':
        return 10;
    case 'B':
    case 'b':
        return 11;
    case 'C':
    case 'c':
        return 12;
    case 'D':
    case 'd':
        return 13;
    case 'E':
    case 'e':
        return 14;
    case 'F':
    case 'f':
        return 15;
    default:
        break;;
    }

    return 0;
}
static int count = 0;

int raw_data_unserialize(char *json_buffer, uint8_t *raw_data, int *raw_data_len)
{
    int attr_len = 0, i = 0;
    char *attr_str = NULL;

    assert(json_buffer && raw_data && raw_data_len);

    attr_str = json_get_value_by_name(json_buffer, strlen(json_buffer),
                                      "rawData", &attr_len, NULL);

    if (!attr_str || !attr_len || attr_len > *raw_data_len * 2)
        return -1;
    // ALINK_LOGD("rawData: %s", attr_str);
    for (i = 0; i < attr_len; i += 2) {
        raw_data[i / 2] = a2x(attr_str[i]) << 4;
        raw_data[i / 2] += a2x(attr_str[i + 1]);
    }

    for (i = 0; i < attr_len; i += 2) {
        raw_data[i / 2] = a2x(attr_str[i]) << 4;
        raw_data[i / 2] += a2x(attr_str[i + 1]);
    }

    raw_data[i / 2] = '\0';
    *raw_data_len = i / 2;

    return 0;
}

int cloud_get_device_raw_data(char *json_buffer)
{
    platform_mutex_lock(xSemDownCmd);
    ALINK_PARAM_CHECK(json_buffer == NULL);
    ALINK_LOGI("The cloud initiates a query to the device");
    alink_err_t ret = ALINK_ERR;
    char *q_data = (char *)malloc(ALINK_DATA_LEN);
    memcpy(q_data, json_buffer, ALINK_DATA_LEN);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) != pdTRUE) {
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        ret = ALINK_ERR;
        if (q_data) {
            free(q_data);
            q_data = NULL;
        }
    }

    platform_mutex_unlock(xSemDownCmd);
    return ret;
}

int cloud_set_device_raw_data(char *json_buffer)
{
    platform_mutex_lock(xSemDownCmd);
    ALINK_PARAM_CHECK(json_buffer == NULL);
    count++;
    ALINK_LOGI("The cloud is set to send instructions");
    alink_err_t ret = ALINK_ERR;
    char *q_data = (char *)malloc(ALINK_DATA_LEN);
    memcpy(q_data, json_buffer, ALINK_DATA_LEN);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) != pdTRUE) {
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        ret = ALINK_ERR;
        if (q_data) {
            free(q_data);
            q_data = NULL;
        }
    }
    platform_mutex_unlock(xSemDownCmd);
    return ret;
}

static void alink_post_data(void *arg)
{
    alink_err_t ret;
    char *up_cmd = NULL;
    for (; post_data_enable;) {
        ret = xQueueReceive(xQueueUpCmd, &up_cmd, portMAX_DELAY);
        if (ret != pdTRUE) {
            ALINK_LOGD("There is no data to report");
            continue;
        }
        // ALINK_LOGD("alink_report:%s", up_cmd);
#ifdef ALINK_PASSTHROUGH
        ret = alink_report(Method_PostRawData, up_cmd);
#else
        ret = alink_report(Method_PostData, up_cmd);
#endif
        if (ret != ALINK_OK) {
            ALINK_LOGW("post failed!");
            platform_msleep(2000);
        } else {
            ALINK_LOGI("dev post data success!");
        }
        if (up_cmd) {
            free(up_cmd);
            up_cmd = NULL;
        }
    }
    vTaskDelete(NULL);
}

void cloud_connected(void)
{
    ALINK_LOGI("alink cloud connected!");
    vTaskDelay(100 / portTICK_RATE_MS);
    if (xSemWriteInfo == NULL) xSemWriteInfo = xSemaphoreCreateBinary();
    xSemaphoreGive(xSemWriteInfo);
}

void cloud_disconnected(void)
{
    ALINK_LOGI("alink cloud disconnected!");
}

static void free_heap_task(void *arg)
{
    while (1) {
        ALINK_LOGD("free heap size: %d", esp_get_free_heap_size());
        ALINK_LOGD("Receive the cloud: %d", count);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void alink_trans_init()
{
    post_data_enable = ALINK_TRUE;
    xSemWrite        = platform_mutex_init();
    xSemRead         = platform_mutex_init();
    xSemDownCmd      = platform_mutex_init();
    xQueueUpCmd      = xQueueCreate(DOWN_CMD_QUEUE_NUM, sizeof(char *));
    xQueueDownCmd    = xQueueCreate(UP_CMD_QUEUE_NUM, sizeof(char *));
    // alink_set_loglevel(ALINK_LL_DEBUG | ALINK_LL_INFO | ALINK_LL_WARN | ALINK_LL_ERROR);
    alink_set_loglevel(ALINK_LL_DEBUG);

    // alink_enable_daily_mode(NULL, 0);
    alink_register_callback(ALINK_CLOUD_CONNECTED, &cloud_connected);
    alink_register_callback(ALINK_CLOUD_DISCONNECTED, &cloud_disconnected);

#ifdef ALINK_PASSTHROUGH
    alink_register_callback(ALINK_GET_DEVICE_RAWDATA, &cloud_get_device_raw_data);
    alink_register_callback(ALINK_SET_DEVICE_RAWDATA, &cloud_set_device_raw_data);
#else
    alink_register_callback(ALINK_GET_DEVICE_STATUS, &cloud_get_device_raw_data);
    alink_register_callback(ALINK_SET_DEVICE_STATUS, &cloud_set_device_raw_data);
#endif
    alink_start();
    ALINK_LOGI("wait main device login");
    /*wait main device login, -1 means wait forever */
    alink_wait_connect(ALINK_WAIT_FOREVER);
    xTaskCreate(alink_post_data, "alink_post_data", 1024 * 4, NULL, DEFAULU_TASK_PRIOTY, NULL);
    xTaskCreate(free_heap_task, "free_heap_task", 1024 * 8, NULL, 3, NULL);
}

void alink_trans_destroy()
{
    post_data_enable = ALINK_FALSE;
    alink_end();
    platform_mutex_destroy(xSemWrite);
    platform_mutex_destroy(xSemRead);
    platform_mutex_destroy(xSemDownCmd);
    platform_mutex_destroy(xSemWriteInfo);
    vQueueDelete(xQueueUpCmd);
    vQueueDelete(xQueueDownCmd);
}

#ifdef ALINK_PASSTHROUGH
#define RawDataHeader   "{\"rawData\":\""
#define RawDataTail     "\", \"length\":\"%d\"}"
static uint8_t raw_data_buffer[ALINK_DATA_LEN];
int esp_write(_IN_ void *up_cmd, size_t len, TickType_t ticks_to_wait)
{
    platform_mutex_lock(xSemWrite);
    ALINK_PARAM_CHECK(up_cmd == NULL);
    ALINK_PARAM_CHECK(len == 0 || len > ALINK_DATA_LEN);

    alink_err_t ret = ALINK_OK;
    int i = 0;
    char *q_data = (char *)malloc(ALINK_DATA_LEN);

    int size = strlen(RawDataHeader);
    strncpy((char *)raw_data_buffer, RawDataHeader, ALINK_DATA_LEN);
    for (i = 0; i < len; i++) {
        size += snprintf((char *)raw_data_buffer + size,
                         ALINK_DATA_LEN - size, "%02X", ((uint8_t *)up_cmd)[i]);
    }

    size += snprintf((char *)raw_data_buffer + size,
                     ALINK_DATA_LEN - size, RawDataTail, len * 2);

    memcpy(q_data, raw_data_buffer, ALINK_DATA_LEN);
    ret = xQueueSend(xQueueUpCmd, &q_data, ticks_to_wait);
    if (ret == pdFALSE) {
        ALINK_LOGW("xQueueSend xQueueUpCmd, wait_time: %d", ticks_to_wait);
        if (q_data) {
            free(q_data);
            q_data = NULL;
        }
    } else {
        ret = size;
    }
    platform_mutex_unlock(xSemWrite);
    return ret;
}

int esp_read(_OUT_ void *down_cmd, size_t size, TickType_t ticks_to_wait)
{
    platform_mutex_lock(xSemRead);
    ALINK_PARAM_CHECK(down_cmd == NULL);
    ALINK_PARAM_CHECK(size == 0 || size > ALINK_DATA_LEN);

    alink_err_t ret = ALINK_ERR;

    char *q_data = NULL;
    ret = xQueueReceive(xQueueDownCmd, &q_data, ticks_to_wait);
    if (ret == pdFALSE) {
        ALINK_LOGE("xQueueReceive xQueueDownCmd, ret:%d, wait_time: %d", ret, ticks_to_wait);
        ret = ALINK_ERR;
        goto EXIT;
    }

    ret = raw_data_unserialize(q_data, (uint8_t *)down_cmd, (int *)&size);
    if (ret != ALINK_OK) {
        ALINK_LOGW("raw_data_unserialize, ret:%d", ret);
        size = ret;
    }
    if (q_data) {
        free(q_data);
        q_data = NULL;
    }
EXIT:
    platform_mutex_unlock(xSemRead);
    return size;
}

#else

int esp_write(_IN_ void *up_cmd, size_t len, TickType_t ticks_to_wait)
{
    platform_mutex_lock(xSemWrite);
    ALINK_PARAM_CHECK(up_cmd == NULL);
    ALINK_PARAM_CHECK(len == 0 || len > ALINK_DATA_LEN);

    alink_err_t ret = ALINK_OK;
    char *q_data = (char *)malloc(ALINK_DATA_LEN);
    memcpy(q_data, up_cmd, len);
    ret = xQueueSend(xQueueUpCmd, &q_data, ticks_to_wait);
    if (ret == pdFALSE) {
        ALINK_LOGW("xQueueSend xQueueUpCmd, wait_time: %d", ticks_to_wait);
        if (q_data) {
            free(q_data);
            q_data = NULL;
        }
    } else {
        ret = len;
    }
    platform_mutex_unlock(xSemWrite);
    return ret;
}

int esp_read(_OUT_ void *down_cmd, size_t size, TickType_t ticks_to_wait)
{
    platform_mutex_lock(xSemRead);
    ALINK_PARAM_CHECK(down_cmd == NULL);
    ALINK_PARAM_CHECK(size == 0 || size > ALINK_DATA_LEN);
    alink_err_t ret = ALINK_ERR;

    char *q_data = NULL;
    ret = xQueueReceive(xQueueDownCmd, &q_data, ticks_to_wait);
    if (ret == pdFALSE) {
        ALINK_LOGE("xQueueReceive xQueueDownCmd, ret:%d, wait_time: %d", ret, ticks_to_wait);
        ret = ALINK_ERR;
        goto EXIT;
    }

    size = strlen(q_data) + 1;
    memcpy(down_cmd, q_data, size);

    if (q_data) {
        free(q_data);
        q_data = NULL;
    }
EXIT:
    platform_mutex_unlock(xSemRead);
    return size;
}
#endif
