
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
#include "alink_export_rawdata.h"
#include "esp_alink.h"
#ifdef ALINK_PASSTHROUGH

#define DOWN_CMD_QUEUE_NUM  5
#define UP_CMD_QUEUE_NUM    5

typedef struct alink_raw_data
{
    char *data;
    size_t len;
} alink_raw_data, *alink_raw_data_ptr;

static const char *TAG = "alink_passthrough";
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
    raw_data->data = (char *)malloc(len);
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

static int rawdata_get_callback(const char *in_rawdata, int in_len, char *out_rawdata, int *out_len)
{
    platform_mutex_lock(xSemDownCmd);
    ALINK_PARAM_CHECK(in_rawdata == NULL);
    ALINK_PARAM_CHECK(in_len <= 0);
    ALINK_LOGI("The cloud initiates a query to the device");
    ALINK_LOGD("in_len: %d", in_len);
    alink_raw_data_ptr q_data = alink_raw_data_malloc(in_len);
    q_data->len = in_len;
    memcpy(q_data->data, in_rawdata, in_len);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) == pdFALSE) {
        alink_raw_data_free(q_data);
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        platform_mutex_unlock(xSemDownCmd);
        return ALINK_ERR;
    }
    platform_mutex_unlock(xSemDownCmd);
    return ALINK_OK;
}

static int rawdata_set_callback(_IN_ char *rawdata, int len)
{
    platform_mutex_lock(xSemDownCmd);
    ALINK_PARAM_CHECK(rawdata == NULL);
    ALINK_PARAM_CHECK(len <= 0);
    ALINK_LOGI("The cloud is set to send instructions");
    alink_raw_data_ptr q_data = alink_raw_data_malloc(len);
    q_data->len = len;
    memcpy(q_data->data, rawdata, len);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) == pdFALSE) {
        alink_raw_data_free(q_data);
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        return ALINK_ERR;
        platform_mutex_unlock(xSemDownCmd);
    }
    platform_mutex_unlock(xSemDownCmd);
    return ALINK_OK;
}

static void alink_post_data(void *arg)
{
    alink_err_t ret;
    alink_raw_data_ptr up_cmd = NULL;
    for (; post_data_enable;) {
        ret = xQueueReceive(xQueueUpCmd, &up_cmd, portMAX_DELAY);
        if (ret == pdFALSE) {
            ALINK_LOGD("There is no data to report");
            continue;
        }

        ALINK_LOGD("alink_device_post_data: param:%p, len: %d", up_cmd->data, up_cmd->len);
        ret = alink_post_device_rawdata(up_cmd->data, up_cmd->len);
        if (ret == ALINK_ERR) {
            ALINK_LOGW("post failed!");
            platform_msleep(2000);
        }
        ALINK_LOGI("dev post data success!");
        if (up_cmd) alink_raw_data_free(up_cmd);
    }
    vTaskDelete(NULL);
}

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
        break;
    default:
        break;
    }
    return ALINK_OK;
}

static void alink_fill_deviceinfo(_OUT_ struct device_info *deviceinfo)
{
    ALINK_PARAM_CHECK(deviceinfo == NULL);
    /*fill main device info here */
    product_get_name(deviceinfo->name);
    product_get_sn(deviceinfo->sn);  // Product registration if it is sn, then need to protect the only sn
    product_get_key(deviceinfo->key);
    product_get_model(deviceinfo->model);
    product_get_secret(deviceinfo->secret);
    product_get_type(deviceinfo->type);
    product_get_version(deviceinfo->version);
    product_get_category(deviceinfo->category);
    product_get_manufacturer(deviceinfo->manufacturer);
    product_get_debug_key(deviceinfo->key_sandbox);
    product_get_debug_secret(deviceinfo->secret_sandbox);
    platform_wifi_get_mac(deviceinfo->mac);//Product registration mac only or sn only unified uppercase
    product_get_cid(deviceinfo->cid); // Use the interface to obtain a unique chipid, anti-counterfeit device
    ALINK_LOGD("DEV_MODEL:%s", deviceinfo->model);
}

void alink_trans_init()
{
    struct device_info *main_dev;
    post_data_enable = ALINK_TRUE;
    main_dev         = platform_malloc(sizeof(struct device_info));
    xSemWrite        = platform_mutex_init();
    xSemRead         = platform_mutex_init();
    xSemDownCmd      = platform_mutex_init();
    xQueueUpCmd      = xQueueCreate(DOWN_CMD_QUEUE_NUM, sizeof(alink_down_cmd_ptr));
    xQueueDownCmd    = xQueueCreate(UP_CMD_QUEUE_NUM, sizeof(alink_down_cmd_ptr));
    memset(main_dev, 0, sizeof(struct device_info));
    alink_fill_deviceinfo(main_dev);
    alink_set_loglevel(ALINK_LL_DEBUG | ALINK_LL_INFO | ALINK_LL_WARN |
                       ALINK_LL_ERROR | ALINK_LL_DUMP);
    // alink_set_loglevel(ALINK_LL_ERROR | ALINK_LL_WARN);
    main_dev->sys_callback[ALINK_FUNC_SERVER_STATUS] = alink_handler_systemstates_callback;

    /*start alink-sdk */
    alink_start_rawdata(main_dev, rawdata_get_callback, rawdata_set_callback);
    platform_free(main_dev);

    ALINK_LOGI("wait main device login");
    /*wait main device login, -1 means wait forever */
    alink_wait_connect(NULL, ALINK_WAIT_FOREVER);
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
