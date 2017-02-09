
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "esp_err.h"
#include "esp_system.h"

#include "platform/platform.h"
#include "product/product.h"
#include "alink_export.h"
#include "esp_alink.h"

#ifndef ALINK_PASSTHROUG

static const char *TAG = "alink_json";
static xQueueHandle xQueueUpCmd = NULL;
static xQueueHandle xQueueDownCmd = NULL;
static SemaphoreHandle_t xSemWrite = NULL;
static SemaphoreHandle_t xSemRead = NULL;

void *alink_sample_mutex;

alink_up_cmd_ptr alink_up_cmd_malloc()
{
    alink_up_cmd_ptr up_cmd = (alink_up_cmd_ptr)malloc(sizeof(alink_up_cmd));
    ALINK_ERROR_CHECK(up_cmd == NULL, NULL, "malloc ret: %p, free_heap: %d", up_cmd, esp_get_free_heap_size());
    memset(up_cmd, 0, sizeof(alink_up_cmd));
    up_cmd->param = (char *)malloc(ALINK_DATA_LEN);
    ALINK_ERROR_CHECK(up_cmd->param == NULL, NULL, "malloc ret: %p, free_heap: %d", up_cmd, esp_get_free_heap_size());
    memset(up_cmd->param, 0, sizeof(ALINK_DATA_LEN));
    return up_cmd;
}

alink_err_t alink_up_cmd_free(_IN_ alink_up_cmd_ptr up_cmd)
{
    ALINK_PARAM_CHECK(up_cmd == NULL);
    if (up_cmd->param) free(up_cmd->param);
    free(up_cmd);
    return ALINK_OK;
}

alink_err_t alink_up_cmd_memcpy(_IN_ alink_up_cmd_ptr dest, _OUT_ alink_up_cmd_ptr src)
{
    ALINK_PARAM_CHECK(dest == NULL);
    ALINK_PARAM_CHECK(src == NULL);
    dest->resp_id     = src->resp_id;
    dest->emergency   = src->emergency;
    if (src->param) memcpy(dest->param, src->param, strlen(src->param) + 1);
    return ALINK_OK;
}

alink_down_cmd_ptr alink_down_cmd_malloc()
{
    alink_down_cmd_ptr down_cmd = (alink_down_cmd_ptr)malloc(sizeof(alink_down_cmd));
    ALINK_ERROR_CHECK(down_cmd == NULL, NULL, "malloc ret: %p, free_heap: %d", down_cmd, esp_get_free_heap_size());
    memset(down_cmd, 0, sizeof(alink_down_cmd));

    down_cmd->account = (char *)malloc(ALINK_DATA_LEN);
    ALINK_ERROR_CHECK(down_cmd->account == NULL, NULL, "malloc ret: %p, free_heap: %d", down_cmd->account, esp_get_free_heap_size());
    down_cmd->param   = (char *)malloc(ALINK_DATA_LEN);
    ALINK_ERROR_CHECK(down_cmd->param == NULL, NULL, "malloc ret: %p, free_heap: %d", down_cmd->param , esp_get_free_heap_size());
    down_cmd->retData = (char *)malloc(ALINK_DATA_LEN);
    ALINK_ERROR_CHECK(down_cmd->retData == NULL, NULL, "malloc ret: %p, free_heap: %d", down_cmd->retData, esp_get_free_heap_size());
    memset(down_cmd->account, 0, sizeof(ALINK_DATA_LEN));
    memset(down_cmd->param, 0, sizeof(ALINK_DATA_LEN));
    memset(down_cmd->retData, 0, sizeof(ALINK_DATA_LEN));
    return down_cmd;
}

alink_err_t alink_down_cmd_free(_IN_ alink_down_cmd_ptr down_cmd)
{
    ALINK_PARAM_CHECK(down_cmd == NULL);
    if (down_cmd->account) free(down_cmd->account);
    if (down_cmd->param) free(down_cmd->param);
    if (down_cmd->retData) free(down_cmd->retData);
    free(down_cmd);
    return ALINK_OK;
}

alink_err_t alink_down_cmd_memcpy(_OUT_ alink_down_cmd_ptr dest, _IN_ alink_down_cmd_ptr src)
{
    ALINK_PARAM_CHECK(dest == NULL);
    ALINK_PARAM_CHECK(src == NULL);
    dest->id     = src->id;
    dest->time   = src->time;
    dest->method = src->method;
    if (src->account) memcpy(dest->account, src->account, strlen(src->account) + 1);
    if (src->param) memcpy(dest->param, src->param, strlen(src->param) + 1);
    if (src->uuid) memcpy(dest->uuid, src->uuid, STR_UUID_LEN);
    if (src->retData) memcpy(dest->retData, src->retData, strlen(src->retData) + 1);
    return ALINK_OK;
}


/* do your job end */
static int alink_handler_systemstates_callback(void *dev_mac, void *sys_state)
{
    char uuid[33] = { 0 };
    char *mac = (char *)dev_mac;
    enum ALINK_STATUS *state = (enum ALINK_STATUS *)sys_state;
    switch (*state) {
    case ALINK_STATUS_INITED:
        break;
    case ALINK_STATUS_REGISTERED:
        sprintf(uuid, "%s", alink_get_uuid(NULL));
        ALINK_LOGI("ALINK_STATUS_REGISTERED, mac %s uuid %s ", mac,
                   uuid);
        break;
    case ALINK_STATUS_LOGGED:
        sprintf(uuid, "%s", alink_get_uuid(NULL));
        ALINK_LOGI("ALINK_STATUS_LOGGED, mac %s uuid %s", mac, uuid);
        break;
    default:
        break;
    }
    return 0;
}

static int main_dev_set_device_status_callback(alink_down_cmd_ptr down_cmd)
{
    ALINK_LOGD("\nuuid:%s\nmethod:%d\nparam:%s", down_cmd->uuid, down_cmd->method, down_cmd->param);
    alink_down_cmd_ptr q_data = alink_down_cmd_malloc();
    alink_down_cmd_memcpy(q_data, down_cmd);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) == pdFALSE) {
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
    }
    return ALINK_OK;
}

static int main_dev_get_device_status_callback(alink_down_cmd_ptr down_cmd)
{
    /* do your job here */
    ALINK_LOGD("\nuuid:%s\nmethod:%d\nparam:%s", down_cmd->uuid, down_cmd->method, down_cmd->param);
    alink_down_cmd_ptr q_data = alink_down_cmd_malloc();
    alink_down_cmd_memcpy(q_data, down_cmd);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) == pdFALSE) {
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
    }
    return ALINK_OK;
    /*do your job end */
}

static int alink_device_post_data(alink_down_cmd_ptr down_cmd)
{
    alink_err_t ret = ALINK_ERR;
    alink_up_cmd_ptr up_cmd = NULL;
    ret = xQueueReceive(xQueueUpCmd, &up_cmd, portMAX_DELAY);
    if (ret == pdFALSE) {
        ALINK_LOGD("There is no data to report");
        ret = ALINK_ERR;
        goto POST_DATA_EXIT;
    }

    if (down_cmd != NULL) {
        up_cmd->target = down_cmd->account;
        up_cmd->resp_id = down_cmd->id;
    } else {
        up_cmd->target = NULL;
        up_cmd->resp_id = -1;
    }

    ret = alink_post_device_data(up_cmd);
    if (ret == ALINK_ERR) {
        ALINK_LOGW("post failed!");
        platform_msleep(2000);
        ret = ALINK_ERR;
        goto POST_DATA_EXIT;
    }
    ALINK_LOGI("dev post data success!");
    ALINK_LOGD("\nparam:%s\n", up_cmd->param);

POST_DATA_EXIT:
    if (up_cmd) alink_up_cmd_free(up_cmd);
    return ret;
}

static void alink_fill_deviceinfo(struct device_info *deviceinfo)
{   /*fill main device info here */
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
    ALINK_LOGI("DEV_MODEL:%s", deviceinfo->model);
}

static int sample_running = ALINK_TRUE;
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
    // alink_set_loglevel(ALINK_LL_ERROR | ALINK_LL_DUMP);
    main_dev->sys_callback[ALINK_FUNC_SERVER_STATUS] = alink_handler_systemstates_callback;


    /*start alink-sdk */

    main_dev->dev_callback[ACB_GET_DEVICE_STATUS] = main_dev_get_device_status_callback;
    main_dev->dev_callback[ACB_SET_DEVICE_STATUS] = main_dev_set_device_status_callback;
    alink_start(main_dev);  /*register main device here */

    platform_free(main_dev);

    ALINK_LOGI("wait main device login");
    /*wait main device login, -1 means wait forever */
    alink_wait_connect(NULL, ALINK_WAIT_FOREVER);

    while (sample_running) {
        alink_device_post_data(NULL);
    }

    alink_end();
    platform_mutex_destroy(alink_sample_mutex);
    vTaskDelete(NULL);
}


alink_err_t alink_write(alink_up_cmd_ptr up_cmd, TickType_t ticks_to_wait)
{
    ALINK_PARAM_CHECK(up_cmd == NULL);
    xSemaphoreTake(xSemWrite, portMAX_DELAY);
    ALINK_LOGI("%p %d\n%s", up_cmd->target, up_cmd->resp_id, up_cmd->param);
    alink_err_t ret = ALINK_ERR;
    alink_up_cmd_ptr q_data = alink_up_cmd_malloc();
    alink_up_cmd_memcpy(q_data, up_cmd);
    ret = xQueueSend(xQueueUpCmd, &q_data, ticks_to_wait);
    if (ret == pdFALSE) {
        ALINK_LOGE("xQueueSend xQueueUpCmd, ret:%d, wait_time: %d", ret, ticks_to_wait);
        xSemaphoreGive(xSemWrite);
        return ALINK_ERR;
    }
    xSemaphoreGive(xSemWrite);
    return ALINK_OK;
}

alink_err_t alink_read(alink_down_cmd_ptr down_cmd, TickType_t ticks_to_wait)
{
    ALINK_PARAM_CHECK(down_cmd == NULL);
    xSemaphoreTake(xSemRead, portMAX_DELAY);
    alink_err_t ret = ALINK_ERR;

    alink_down_cmd_ptr q_data = NULL;
    ret = xQueueReceive(xQueueDownCmd, &q_data, ticks_to_wait);
    if (ret == pdFALSE) {
        ALINK_LOGE("xQueueReceive xQueueDownCmd, ret:%d, wait_time: %d", ret, ticks_to_wait);
        xSemaphoreGive(xSemRead);
        return ALINK_ERR;
    }
    alink_down_cmd_memcpy(down_cmd, q_data);
    alink_down_cmd_free(q_data);
    xSemaphoreGive(xSemRead);
    return ALINK_OK;
}

int esp_write(char *up_cmd, size_t size, TickType_t ticks_to_wait)
{
    ALINK_PARAM_CHECK(up_cmd == NULL);
    ALINK_PARAM_CHECK(size == 0 || size > ALINK_DATA_LEN);
    size_t param_size = ALINK_DATA_LEN;
    xSemaphoreTake(xSemWrite, portMAX_DELAY);
    alink_err_t ret = ALINK_ERR;
    alink_up_cmd_ptr q_data = alink_up_cmd_malloc();
    if(size < param_size) param_size = size;

    memcpy(q_data->param, up_cmd, param_size);
    ret = xQueueSend(xQueueUpCmd, &q_data, ticks_to_wait);
    if (ret == pdFALSE) {
        ALINK_LOGD("xQueueSend xQueueUpCmd, ret:%d, wait_time: %d", ret, ticks_to_wait);
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

    alink_down_cmd_ptr q_data = NULL;
    ret = xQueueReceive(xQueueDownCmd, &q_data, ticks_to_wait);
    if (ret == pdFALSE) {
        ALINK_LOGE("xQueueReceive xQueueDownCmd, ret:%d, wait_time: %d", ret, ticks_to_wait);
        xSemaphoreGive(xSemRead);
        return ALINK_ERR;
    }

    param_size = strlen(q_data->param) + 1;
    if(size < param_size) param_size = size;

    memcpy(down_cmd, q_data->param, param_size);
    alink_down_cmd_free(q_data);
    xSemaphoreGive(xSemRead);
    return param_size;
}
#endif
