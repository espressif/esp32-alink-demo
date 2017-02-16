
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
#ifdef ALINK_PASSTHROUGH

#define DOWN_CMD_QUEUE_NUM  5
#define UP_CMD_QUEUE_NUM    5

#define Method_PostData    "postDeviceData"
#define Method_PostRawData "postDeviceRawData"
#define Method_GetAlinkTime "getAlinkTime"
#define post_data_buffer_size    (512)
static uint8_t post_data_buffer[post_data_buffer_size];
static uint8_t raw_data_buffer[post_data_buffer_size];


typedef struct alink_raw_data
{
    uint8_t *data;
    int len;
} alink_raw_data, *alink_raw_data_ptr;

static const char *TAG = "alink_passthrough";
extern SemaphoreHandle_t xSemWriteInfo;
static alink_err_t post_data_enable = ALINK_TRUE;

static xQueueHandle xQueueDownCmd    = NULL;
static xQueueHandle xQueueUpCmd      = NULL;
static SemaphoreHandle_t xSemWrite   = NULL;
static SemaphoreHandle_t xSemRead    = NULL;
static SemaphoreHandle_t xSemDownCmd = NULL;

alink_raw_data_ptr alink_raw_data_malloc(size_t len)
{
    ALINK_PARAM_CHECK(len > ALINK_DATA_LEN);
    alink_raw_data_ptr raw_data = (alink_raw_data_ptr)malloc(sizeof(alink_raw_data));
    ALINK_ERROR_CHECK(raw_data == NULL, NULL, "malloc ret: %p, free_heap: %d", raw_data, esp_get_free_heap_size());
    memset(raw_data, 0, sizeof(alink_raw_data));
    raw_data->data = (uint8_t *)malloc(len);
    ALINK_ERROR_CHECK(raw_data->data == NULL, NULL, "malloc ret: %p, free_heap: %d", raw_data->data, esp_get_free_heap_size());
    memset(raw_data->data, 0, len);
    return raw_data;
}

alink_err_t alink_raw_data_free(_IN_ alink_raw_data_ptr raw_data)
{
    ALINK_PARAM_CHECK(raw_data == NULL);
    if (raw_data->data) free(raw_data->data);
    free(raw_data);
    return ALINK_OK;
}


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

int raw_data_unserialize(char *json_buffer, uint8_t *raw_data, int *raw_data_len)
{
    int attr_len = 0, i = 0;
    char *attr_str = NULL;

    assert(json_buffer && raw_data && raw_data_len);

    attr_str = json_get_value_by_name(json_buffer, strlen(json_buffer),
                                      "rawData", &attr_len, NULL);

    if (!attr_str || !attr_len || attr_len > *raw_data_len * 2)
        return -1;

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
    alink_raw_data_ptr q_data = NULL;

    q_data = alink_raw_data_malloc(ALINK_DATA_LEN);
    if (q_data == NULL) {
        ALINK_LOGW("alink_raw_data_malloc is err, free_heap: %d", esp_get_free_heap_size());
        goto EXIT;
    }

    ret = raw_data_unserialize(json_buffer, q_data->data, &q_data->len);
    if (ret != ALINK_OK) {
        ALINK_LOGW("raw_data_unserialize, ret:%d", ret);
        goto EXIT;
    }

    if (xQueueSend(xQueueDownCmd, &q_data, 0) != pdTRUE) {
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        ret = ALINK_ERR;
        goto EXIT;
    }

EXIT:
    if (q_data) alink_raw_data_free(q_data);
    platform_mutex_unlock(xSemDownCmd);
    return ret;
}

int cloud_set_device_raw_data(char *json_buffer)
{
    platform_mutex_lock(xSemDownCmd);
    ALINK_PARAM_CHECK(json_buffer == NULL);
    ALINK_LOGI("The cloud is set to send instructions");
    alink_err_t ret = ALINK_ERR;
    alink_raw_data_ptr q_data = NULL;

    q_data = alink_raw_data_malloc(ALINK_DATA_LEN);
    if (q_data == NULL) {
        ALINK_LOGW("alink_raw_data_malloc is err, free_heap: %d", esp_get_free_heap_size());
        goto EXIT;
    }

    ret = raw_data_unserialize(json_buffer, q_data->data, &q_data->len);
    if (ret != ALINK_OK) {
        ALINK_LOGW("raw_data_unserialize, ret:%d", ret);
        goto EXIT;
    }

    if (xQueueSend(xQueueDownCmd, &q_data, 0) != pdTRUE) {
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        ret = ALINK_ERR;
        goto EXIT;
    }

EXIT:
    if (q_data) alink_raw_data_free(q_data);
    platform_mutex_unlock(xSemDownCmd);
    return ret;
}

#define RawDataHeader   "{\"rawData\":\""
#define RawDataTail     "\", \"length\":\"%d\"}"
static void alink_post_data(void *arg)
{
    alink_err_t ret;
    alink_raw_data_ptr up_cmd = NULL;
    size_t size;
    int i = 0;
    for (; post_data_enable;) {
        ret = xQueueReceive(xQueueUpCmd, &up_cmd, portMAX_DELAY);
        if (ret == pdFALSE) {
            ALINK_LOGD("There is no data to report");
            continue;
        }

        ALINK_LOGD("alink_device_post_data: param:%p, len: %d", up_cmd->data, up_cmd->len);
        size = strlen(RawDataHeader);
        strncpy((char *)raw_data_buffer, RawDataHeader, post_data_buffer_size);
        for (i = 0; i < up_cmd->len; i++) {
            size += snprintf((char *)raw_data_buffer + size,
                             post_data_buffer_size - size, "%02X", up_cmd->data[i]);
        }

        size += snprintf((char *)raw_data_buffer + size,
                         post_data_buffer_size - size, RawDataTail, up_cmd->len * 2);

        ret = alink_report(Method_PostRawData, (char *)raw_data_buffer);
        if (ret == ALINK_ERR) {
            ALINK_LOGW("post failed!");
            platform_msleep(2000);
        }
        ALINK_LOGI("dev post data success!");
        if (up_cmd) alink_raw_data_free(up_cmd);
    }
    vTaskDelete(NULL);
}

#if 0
static int alink_handler_systemstates_callback(_IN_ void *dev_mac, _IN_ void *sys_state)
{
    ALINK_PARAM_CHECK(dev_mac == NULL);
    ALINK_PARAM_CHECK(sys_state == NULL);

    enum ALINK_STATUS *state = (enum ALINK_STATUS *)sys_state;
    switch (*state) {
    case ALINK_STATUS_INITED:
        ALINK_LOGI("ALINK_STATUS_INITED, mac %s uuid %s", (char *)dev_mac, alink_get_uuid(NULL));
        break;
    case ALINK_STATUS_REGISTERED:
        ALINK_LOGI("ALINK_STATUS_REGISTERED, mac %s uuid %s", (char *)dev_mac, alink_get_uuid(NULL));
        break;
    case ALINK_STATUS_LOGGED:
        ALINK_LOGI("ALINK_STATUS_LOGGED, mac %s uuid %s", (char *)dev_mac, alink_get_uuid(NULL));
        vTaskDelay(100 / portTICK_RATE_MS);
        if (xSemWriteInfo == NULL) xSemWriteInfo = xSemaphoreCreateBinary();
        xSemaphoreGive(xSemWriteInfo);
        break;
    default:
        break;
    }
    return ALINK_OK;
}
#endif
void cloud_connected(void) { printf("alink cloud connected!\n"); }
void cloud_disconnected(void) { printf("alink cloud disconnected!\n"); }

void alink_trans_init()
{
    post_data_enable = ALINK_TRUE;
    xSemWrite        = platform_mutex_init();
    xSemRead         = platform_mutex_init();
    xSemDownCmd      = platform_mutex_init();
    xQueueUpCmd      = xQueueCreate(DOWN_CMD_QUEUE_NUM, sizeof(char *));
    xQueueDownCmd    = xQueueCreate(UP_CMD_QUEUE_NUM, sizeof(char *));
    alink_set_loglevel(ALINK_LL_DEBUG | ALINK_LL_INFO | ALINK_LL_WARN | ALINK_LL_ERROR);
    // alink_set_loglevel(ALINK_LL_ERROR | ALINK_LL_WARN);

    alink_enable_daily_mode(NULL, 0);
    alink_register_callback(ALINK_CLOUD_CONNECTED, &cloud_connected);
    alink_register_callback(ALINK_CLOUD_DISCONNECTED, &cloud_disconnected);
    alink_register_callback(ALINK_GET_DEVICE_RAWDATA, &cloud_get_device_raw_data);
    alink_register_callback(ALINK_SET_DEVICE_RAWDATA, &cloud_set_device_raw_data);
    alink_start();
    ALINK_LOGI("wait main device login");
    /*wait main device login, -1 means wait forever */
    alink_wait_connect(ALINK_WAIT_FOREVER);
    xTaskCreate(alink_post_data, "alink_post_data", 1024 * 4, NULL, DEFAULU_TASK_PRIOTY, NULL);
}


void alink_trans_destroy()
{
    post_data_enable = ALINK_FALSE;
    alink_end();
    platform_mutex_destroy(xSemWrite);
    platform_mutex_destroy(xSemRead);
    platform_mutex_destroy(xSemDownCmd);
    vQueueDelete(xQueueUpCmd);
    vQueueDelete(xQueueDownCmd);
}


int esp_write(_IN_ void *up_cmd, size_t size, TickType_t ticks_to_wait)
{
    platform_mutex_lock(xSemWrite);
    ALINK_PARAM_CHECK(up_cmd == NULL);
    ALINK_PARAM_CHECK(size == 0 || size > ALINK_DATA_LEN);

    alink_err_t ret = ALINK_ERR;
    alink_raw_data_ptr q_data = alink_raw_data_malloc(ALINK_DATA_LEN);
    q_data->len = ALINK_DATA_LEN;
    if (size < q_data->len) q_data->len = size;

    memcpy(q_data->data, up_cmd, q_data->len);
    ret = xQueueSend(xQueueUpCmd, &q_data, ticks_to_wait);
    if (ret == pdFALSE) {
        ALINK_LOGW("xQueueSend xQueueUpCmd, wait_time: %d", ticks_to_wait);
        alink_raw_data_free(q_data);
        platform_mutex_unlock(xSemWrite);
        return ALINK_ERR;
    }
    platform_mutex_unlock(xSemWrite);
    return ALINK_OK;
}

int esp_read(_OUT_ void *down_cmd, size_t size, TickType_t ticks_to_wait)
{
    platform_mutex_lock(xSemRead);
    ALINK_PARAM_CHECK(down_cmd == NULL);
    ALINK_PARAM_CHECK(size == 0 || size > ALINK_DATA_LEN);

    alink_err_t ret = ALINK_ERR;

    alink_raw_data_ptr q_data = NULL;
    ret = xQueueReceive(xQueueDownCmd, &q_data, ticks_to_wait);
    if (ret == pdFALSE) {
        ALINK_LOGE("xQueueReceive xQueueDownCmd, ret:%d, wait_time: %d", ret, ticks_to_wait);
        platform_mutex_unlock(xSemRead);
        return ALINK_ERR;
    }

    if (q_data->len < size) size = q_data->len;
    memcpy(down_cmd, q_data->data, size);
    alink_raw_data_free(q_data);
    platform_mutex_unlock(xSemRead);
    return size;
}
#endif
