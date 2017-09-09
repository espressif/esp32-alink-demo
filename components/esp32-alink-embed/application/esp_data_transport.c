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

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#include "alink_platform.h"
#include "alink_product.h"
#include "alink_export.h"

#include "esp_alink.h"
#include "esp_alink_log.h"
#include "esp_json_parser.h"

#define Method_PostData     "postDeviceData"
#define Method_PostRawData  "postDeviceRawData"
#define Method_GetStatus    "getDeviceStatus"
#define Method_SetStatus    "setDeviceStatus"
#define Method_GetAlinkTime "getAlinkTime"

#define ALINK_METHOD_POST     Method_PostData
#define ALINK_GET_DEVICE_DATA ALINK_GET_DEVICE_STATUS
#define ALINK_SET_DEVICE_DATA ALINK_SET_DEVICE_STATUS

static const char *TAG              = "esp_data_transport.c";
static alink_err_t post_data_enable = ALINK_TRUE;
static xQueueHandle xQueueDownCmd   = NULL;
static xQueueHandle xQueueUpCmd     = NULL;

static alink_err_t cloud_get_device_data(_IN_ char *json_buffer)
{
    ALINK_PARAM_CHECK(json_buffer);

    alink_err_t ret = ALINK_OK;

    alink_event_send(ALINK_EVENT_GET_DEVICE_DATA);

    ret = esp_json_pack(json_buffer, "method", Method_GetStatus);
    ALINK_ERROR_CHECK(ret < 0, ALINK_ERR, "esp_json_pack, ret:%d", ret);

    int size = ret + 1;
    char *q_data = (char *)alink_malloc(size);

    if (size > ALINK_DATA_LEN) {
        ALINK_LOGW("json_buffer len:%d", size);
    }

    memcpy(q_data, json_buffer, size);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) != pdTRUE) {
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        ret = ALINK_ERR;
        alink_free(q_data);
    }

    return ret;
}

static alink_err_t cloud_set_device_data(_IN_ char *json_buffer)
{
    ALINK_PARAM_CHECK(json_buffer);

    alink_err_t ret = ALINK_OK;

    alink_event_send(ALINK_EVENT_SET_DEVICE_DATA);

    ret = esp_json_pack(json_buffer, "method", Method_SetStatus);
    ALINK_ERROR_CHECK(ret < 0, ALINK_ERR, "esp_json_pack, ret:%d", ret);

    int size = ret + 1;
    char *q_data = (char *)alink_malloc(size);

    if (size > ALINK_DATA_LEN) {
        ALINK_LOGW("json_buffer len:%d, json_buffer: %s", size, json_buffer);
    }

    memcpy(q_data, json_buffer, size);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) != pdTRUE) {
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        ret = ALINK_ERR;
        alink_free(q_data);
    }

    return ret;
}

#ifndef ALINK_WRITE_NOT_BUFFER
static void alink_post_data(void *arg)
{
    alink_err_t ret;
    char *up_cmd = NULL;

    for (; post_data_enable;) {
        ret = xQueueReceive(xQueueUpCmd, &up_cmd, portMAX_DELAY);

        if (ret != pdTRUE) {
            ALINK_LOGW("There is no data to report");
            continue;
        }

        ALINK_LOGV("up_cmd: %s", up_cmd);
        ret = alink_report(ALINK_METHOD_POST, up_cmd);

        if (ret != ALINK_OK) {
            ALINK_LOGW("post failed!");
            alink_event_send(ALINK_EVENT_POST_CLOUD_DATA_FAIL);
            platform_msleep(3000);
        } else {
            alink_event_send(ALINK_EVENT_POST_CLOUD_DATA);
        }

        alink_free(up_cmd);
    }

    vTaskDelete(NULL);
}
#endif /*!< ALINK_WRITE_NOT_BUFFER */

static void cloud_connected(void)
{
    alink_event_send(ALINK_EVENT_CLOUD_CONNECTED);
}

static void cloud_disconnected(void)
{
    alink_event_send(ALINK_EVENT_CLOUD_DISCONNECTED);
}

alink_err_t alink_trans_init()
{
    alink_err_t ret  = ALINK_OK;
    post_data_enable = ALINK_TRUE;
    xQueueUpCmd      = xQueueCreate(DOWN_CMD_QUEUE_NUM, sizeof(char *));
    xQueueDownCmd    = xQueueCreate(UP_CMD_QUEUE_NUM, sizeof(char *));
    alink_set_loglevel(ALINK_SDK_LOG_LEVEL);

    alink_register_callback(ALINK_CLOUD_CONNECTED, &cloud_connected);
    alink_register_callback(ALINK_CLOUD_DISCONNECTED, &cloud_disconnected);
    alink_register_callback(ALINK_GET_DEVICE_DATA, &cloud_get_device_data);
    alink_register_callback(ALINK_SET_DEVICE_DATA, &cloud_set_device_data);

    ret = alink_start();
    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "alink_start :%d", ret);
    ALINK_LOGI("wait main device login");
    /*wait main device login, -1 means wait forever */
    ret = alink_wait_connect(ALINK_WAIT_FOREVER);
    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "alink_start : %d", ret);

#ifndef ALINK_WRITE_NOT_BUFFER
    ret = xTaskCreate(alink_post_data, "alink_post_data", ALINK_POST_DATA_STACK_SIZE, NULL, DEFAULU_TASK_PRIOTY, NULL);
    ALINK_ERROR_CHECK(ret != pdTRUE, ALINK_ERR, "thread_create name: %s, stack_size: %d, ret: %d", "alink_post_data", 2048, ret);
#endif /*!< ALINK_WRITE_NOT_BUFFER */

    return ALINK_OK;
}

void alink_trans_destroy()
{
    post_data_enable = ALINK_FALSE;
    alink_end();
    vQueueDelete(xQueueUpCmd);
    vQueueDelete(xQueueDownCmd);
}

#ifdef ALINK_WRITE_NOT_BUFFER
ssize_t alink_write(_IN_ const void *up_cmd, size_t size, int micro_seconds)
{
    ALINK_PARAM_CHECK(up_cmd);
    ALINK_PARAM_CHECK(size && size <= ALINK_DATA_LEN);
    alink_err_t ret = 0;

    ALINK_LOGV("up_cmd: %s", (char *)up_cmd);
    ret = alink_report(ALINK_METHOD_POST, up_cmd);

    if (ret != ALINK_OK) {
        ALINK_LOGW("post failed!");
        platform_msleep(1000);
    } else {
        alink_event_send(ALINK_EVENT_POST_CLOUD_DATA);
    }
    return size;
}
#endif

#ifndef ALINK_WRITE_NOT_BUFFER
ssize_t alink_write(_IN_ const void *up_cmd, size_t size, int micro_seconds)
{
    ALINK_PARAM_CHECK(up_cmd);
    ALINK_PARAM_CHECK(size && size <= ALINK_DATA_LEN);

    if (!xQueueUpCmd) {
        return ALINK_ERR;
    }

    alink_err_t ret = ALINK_OK;
    char *q_data = (char *)alink_malloc(size);
    memcpy(q_data, up_cmd, size);

    ret = xQueueSend(xQueueUpCmd, &q_data, micro_seconds / portTICK_RATE_MS);

    if (ret == pdFALSE) {
        ALINK_LOGD("xQueueSend xQueueUpCmd, wait_time: %d", micro_seconds);
        alink_free(q_data);
        return ALINK_ERR;
    }

    return size;
}
#endif

ssize_t alink_read(_OUT_ void *down_cmd, size_t size, int micro_seconds)
{
    ALINK_PARAM_CHECK(down_cmd);
    ALINK_PARAM_CHECK(xQueueDownCmd);
    ALINK_PARAM_CHECK(size && size <= ALINK_DATA_LEN);

    alink_err_t ret = ALINK_OK;
    char *q_data = NULL;

    ret = xQueueReceive(xQueueDownCmd, &q_data, micro_seconds / portTICK_RATE_MS);

    if (ret == pdFALSE) {
        ALINK_LOGD("xQueueReceive xQueueDownCmd, ret:%d, wait_time: %d", ret, micro_seconds);
        alink_free(q_data);
        return ALINK_ERR;
    }

    int size_tmp = strlen(q_data) + 1;
    size = (size_tmp > size) ? size : size_tmp;

    if (size > ALINK_DATA_LEN) {
        ALINK_LOGD("read len > ALINK_DATA_LEN, len: %d", size);
        size = ALINK_DATA_LEN;
        q_data[size - 1] = '\0';
    }

    memcpy(down_cmd, q_data, size);

    alink_free(q_data);
    return size;
}
