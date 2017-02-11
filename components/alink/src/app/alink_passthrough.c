
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

typedef struct alink_raw_data
{
    char *data;
    size_t len;
} alink_raw_data, *alink_raw_data_ptr;

static const char *TAG = "alink_passthrough";

static xQueueHandle xQueueUpCmd = NULL;
static xQueueHandle xQueueDownCmd = NULL;
static SemaphoreHandle_t xSemWrite = NULL;
static SemaphoreHandle_t xSemRead = NULL;
static SemaphoreHandle_t alink_sample_mutex = NULL;

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


static int alink_device_post_raw_data(void)
{
    alink_err_t ret;
    alink_raw_data_ptr up_cmd = NULL;
    ret = xQueueReceive(xQueueUpCmd, &up_cmd, portMAX_DELAY);
    if (ret == pdFALSE) {
        ALINK_LOGD("There is no data to report");
        ret = ALINK_ERR;
        goto POST_RAW_DATA_EXIT;
    }

    ret = alink_post_device_rawdata(up_cmd->data, up_cmd->len);
    if (ret == ALINK_ERR) {
        ALINK_LOGW("post failed!");
        platform_msleep(2000);
        ret = ALINK_ERR;
        goto POST_RAW_DATA_EXIT;
    }
    ALINK_LOGI("dev post data success!");

POST_RAW_DATA_EXIT:
    if(up_cmd) alink_raw_data_free(up_cmd);
    return ret;
}

static int rawdata_get_callback(const char *in_rawdata, int in_len, char *out_rawdata, int *out_len)
{
    ALINK_PARAM_CHECK(in_rawdata == NULL);
    ALINK_PARAM_CHECK(in_len <= 0);
    ALINK_LOGI("in_rawdata: %02x %02x %d %d %d %d %d %02x",
               in_rawdata[0], in_rawdata[1], in_rawdata[2], in_rawdata[3],
               in_rawdata[4], in_rawdata[5], in_rawdata[6], in_rawdata[7]);
    alink_raw_data_ptr q_data = alink_raw_data_malloc(in_len);
    q_data->len = in_len;
    memcpy(q_data->data, in_rawdata, in_len);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) == pdFALSE) {
        alink_raw_data_free(q_data);
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        return ALINK_ERR;
    }
    return ALINK_OK;
}

static int rawdata_set_callback(_IN_ char *rawdata, int len)
{
    ALINK_PARAM_CHECK(rawdata == NULL);
    ALINK_PARAM_CHECK(len <= 0);

    alink_raw_data_ptr q_data = alink_raw_data_malloc(len);
    q_data->len = len;
    memcpy(q_data->data, rawdata, len);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) == pdFALSE) {
        alink_raw_data_free(q_data);
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        return ALINK_ERR;
    }
    return ALINK_OK;
}

static int alink_handler_systemstates_callback(_IN_ void *dev_mac, _IN_ void *sys_state)
{
    ALINK_PARAM_CHECK(dev_mac == NULL);
    ALINK_PARAM_CHECK(sys_state == NULL);

    char uuid[33] = { 0 };
    char *mac = (char *)dev_mac;
    enum ALINK_STATUS *state = (enum ALINK_STATUS *)sys_state;
    switch (*state) {
    case ALINK_STATUS_INITED:

        break;
    case ALINK_STATUS_REGISTERED:
        sprintf(uuid, "%s", alink_get_uuid(NULL));
        ALINK_LOGI("ALINK_STATUS_REGISTERED, mac %s uuid %s \n", mac,
                 uuid);
        break;
    case ALINK_STATUS_LOGGED:
        sprintf(uuid, "%s", alink_get_uuid(NULL));
        ALINK_LOGI("ALINK_STATUS_LOGGED, mac %s uuid %s\n", mac, uuid);
        break;
    default:
        break;
    }
    return 0;
}


static void alink_fill_deviceinfo(_OUT_ struct device_info *deviceinfo)
{
    ALINK_PARAM_CHECK(deviceinfo == NULL);
    /*fill main device info here */
    product_get_name(deviceinfo->name);
    product_get_sn(deviceinfo->sn);  // 产品注册方式 如果是sn, 那么需要保障sn唯一
    product_get_key(deviceinfo->key);
    product_get_model(deviceinfo->model);
    product_get_secret(deviceinfo->secret);
    product_get_type(deviceinfo->type);
    product_get_version(deviceinfo->version);
    product_get_category(deviceinfo->category);
    product_get_manufacturer(deviceinfo->manufacturer);
    product_get_debug_key(deviceinfo->key_sandbox);
    product_get_debug_secret(deviceinfo->secret_sandbox);
    platform_wifi_get_mac(deviceinfo->mac);//产品注册mac唯一 or sn唯一  统一大写
    product_get_cid(deviceinfo->cid); // 使用接口获取唯一chipid,防伪造设备

    ALINK_LOGI("DEV_MODEL:%s", deviceinfo->model);
}

void alink_trans_init(void *arg)
{
    struct device_info *main_dev;
    main_dev = platform_malloc(sizeof(struct device_info));
    alink_sample_mutex = platform_mutex_init();
    xQueueUpCmd = xQueueCreate(3, sizeof(alink_up_cmd_ptr));
    xQueueDownCmd = xQueueCreate(3, sizeof(alink_down_cmd_ptr));
    xSemWrite = xSemaphoreCreateMutex();
    xSemRead = xSemaphoreCreateMutex();
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

    for(;;) alink_device_post_raw_data();

    alink_end();
    platform_mutex_destroy(alink_sample_mutex);
    vTaskDelete(NULL);
}

int esp_write(char *up_cmd, size_t size, TickType_t ticks_to_wait)
{
    ALINK_PARAM_CHECK(up_cmd == NULL);
    ALINK_PARAM_CHECK(size == 0 || size > ALINK_DATA_LEN);

    xSemaphoreTake(xSemWrite, portMAX_DELAY);
    alink_err_t ret = ALINK_ERR;
    alink_raw_data_ptr q_data = alink_raw_data_malloc(ALINK_DATA_LEN);
    q_data->len = ALINK_DATA_LEN;
    if(size < q_data->len) q_data->len = size;

    memcpy(q_data->data, up_cmd, q_data->len);
    ret = xQueueSend(xQueueUpCmd, &q_data, ticks_to_wait);
    if (ret == pdFALSE) {
        ALINK_LOGW("xQueueSend xQueueUpCmd, ret:%d, wait_time: %d", ret, ticks_to_wait);
        alink_raw_data_free(q_data);
        xSemaphoreGive(xSemWrite);
        return ALINK_ERR;
    }
    xSemaphoreGive(xSemWrite);
    return ALINK_OK;
}

int esp_read(char *down_cmd, size_t size, TickType_t ticks_to_wait)
{
    ALINK_PARAM_CHECK(down_cmd == NULL);
    ALINK_PARAM_CHECK(size == 0 || size > ALINK_DATA_LEN);

    xSemaphoreTake(xSemRead, portMAX_DELAY);
    alink_err_t ret = ALINK_ERR;
    size_t param_size = 0;

    alink_raw_data_ptr q_data = NULL;
    ret = xQueueReceive(xQueueDownCmd, &q_data, ticks_to_wait);
    if (ret == pdFALSE) {
        ALINK_LOGE("xQueueReceive xQueueDownCmd, ret:%d, wait_time: %d", ret, ticks_to_wait);
        xSemaphoreGive(xSemRead);
        return ALINK_ERR;
    }

    param_size = q_data->len;
    memcpy(down_cmd, q_data->data, param_size);
    alink_raw_data_free(q_data);
    xSemaphoreGive(xSemRead);
    return param_size;
}
#endif
