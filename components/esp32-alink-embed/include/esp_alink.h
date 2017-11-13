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

#ifndef __ESP_ALINK_H__
#define __ESP_ALINK_H__

#include "esp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t alink_err_t;

#ifndef ALINK_TRUE
#define ALINK_TRUE  1
#endif
#ifndef ALINK_FALSE
#define ALINK_FALSE 0
#endif
#ifndef ALINK_OK
#define ALINK_OK    0
#endif
#ifndef ALINK_ERR
#define ALINK_ERR   -1
#endif

#ifndef _IN_
#define _IN_            /*!< indicate that this is a input parameter. */
#endif
#ifndef _OUT_
#define _OUT_           /*!< indicate that this is a output parameter. */
#endif
#ifndef _INOUT_
#define _INOUT_         /*!< indicate that this is a io parameter. */
#endif
#ifndef _IN_OPT_
#define _IN_OPT_        /*!< indicate that this is a optional input parameter. */
#endif
#ifndef _OUT_OPT_
#define _OUT_OPT_       /*!< indicate that this is a optional output parameter. */
#endif
#ifndef _INOUT_OPT_
#define _INOUT_OPT_     /*!< indicate that this is a optional io parameter. */
#endif

#ifndef CONFIG_WIFI_WAIT_TIME
#define CONFIG_WIFI_WAIT_TIME             60
#endif

#ifndef CONFIG_ALINK_DATA_LEN
#define CONFIG_ALINK_DATA_LEN             2048
#endif

#ifndef CONFIG_ALINK_TASK_PRIOTY
#define CONFIG_ALINK_TASK_PRIOTY          6
#endif

#ifndef CONFIG_DOWN_CMD_QUEUE_NUM
#define CONFIG_DOWN_CMD_QUEUE_NUM         3
#endif

#ifndef CONFIG_UP_CMD_QUEUE_NUM
#define CONFIG_UP_CMD_QUEUE_NUM           3
#endif

#ifndef CONFIG_EVENT_STACK_SIZE
#define CONFIG_EVENT_STACK_SIZE           4096
#endif

#ifndef CONFIG_ALINK_POST_DATA_STACK_SIZE
#define CONFIG_ALINK_POST_DATA_STACK_SIZE 4096
#endif

#ifndef CONFIG_ALINK_CHIPID
#define CONFIG_ALINK_CHIPID               "esp32"
#endif

#ifndef CONFIG_ALINK_MODULE_NAME
#define CONFIG_ALINK_MODULE_NAME          "ESP-WROOM-32"
#endif

#define ALINK_CHIPID                      CONFIG_ALINK_CHIPID
#define ALINK_MODULE_NAME                 CONFIG_ALINK_MODULE_NAME

#define WIFI_WAIT_TIME                    (CONFIG_WIFI_WAIT_TIME * 1000)
#define ALINK_DATA_LEN                    CONFIG_ALINK_DATA_LEN
#define DEFAULU_TASK_PRIOTY               CONFIG_ALINK_TASK_PRIOTY
#define ALINK_EVENT_STACK_SIZE            CONFIG_ALINK_EVENT_STACK_SIZE
#define ALINK_POST_DATA_STACK_SIZE        CONFIG_ALINK_POST_DATA_STACK_SIZE

#define DOWN_CMD_QUEUE_NUM                CONFIG_DOWN_CMD_QUEUE_NUM
#define UP_CMD_QUEUE_NUM                  CONFIG_UP_CMD_QUEUE_NUM

#ifdef  CONFIG_ALINK_WRITE_NOT_BUFFER
#define ALINK_WRITE_NOT_BUFFER
#endif

typedef enum {
    ALINK_EVENT_CLOUD_CONNECTED = 0,/*!< ESP32 connected from alink cloude */
    ALINK_EVENT_CLOUD_DISCONNECTED, /*!< ESP32 disconnected from alink cloude */
    ALINK_EVENT_GET_DEVICE_DATA,    /*!< Alink cloud requests data from the device */
    ALINK_EVENT_SET_DEVICE_DATA,    /*!< Alink cloud to send data to the device */
    ALINK_EVENT_POST_CLOUD_DATA,    /*!< The device sends data to alink cloud  */
    ALINK_EVENT_POST_CLOUD_DATA_FAIL, /*!< The device sends data to alink cloud fialed  */
    ALINK_EVENT_WIFI_CONNECTED,     /*!< ESP32 station got IP from connected AP */
    ALINK_EVENT_WIFI_DISCONNECTED,  /*!< ESP32 station disconnected from AP */
    ALINK_EVENT_CONFIG_NETWORK,     /*!< The equipment enters the distribution mode */
    ALINK_EVENT_UPDATE_ROUTER,      /*!< Request to configure the router */
    ALINK_EVENT_FACTORY_RESET,      /*!< Request to restore factory settings */
    ALINK_EVENT_ACTIVATE_DEVICE,    /*!< Request activation device */
} alink_event_t;

typedef struct alink_product {
    const char *name;
    const char *model;
    const char *version;
    const char *key;
    const char *secret;
    const char *key_sandbox;
    const char *secret_sandbox;
    const char *key_device;
    const char *secret_device;
} alink_product_t;

#define alink_malloc(num_bytes) ({void *ptr = malloc(num_bytes); \
        ALINK_LOGV("malloc size: %d, ptr: %p, heap free: %d", num_bytes, ptr, esp_get_free_heap_size()); ptr;})

#define alink_calloc(n, size) ({void *ptr = calloc(n, size); \
        ALINK_LOGV("calloc size: %d, ptr: %p, heap free: %d", n * size, ptr, esp_get_free_heap_size()); ptr;})

#define alink_free(arg) {if(arg){ free(arg); \
        ALINK_LOGV("free ptr: %p, heap free: %d", arg, esp_get_free_heap_size()); arg = NULL;}}while(0)

/**
 * @brief  Application specified event callback function
 *
 * @param  event event type defined in this file
 *
 * @note The memory space for the event callback function defaults to 4kBety
 *
 * @return
 *     - ALINK_OK : Succeed
 *     - others : fail
 */
typedef alink_err_t (*alink_event_cb_t)(alink_event_t event);

/**
 * @brief  Send the event to the event handler
 *
 * @param  event  Generated events
 *
 * @return
 *     - ALINK_OK : Succeed
 *     - ALINK_ERR :   Fail
 */
alink_err_t alink_event_send(alink_event_t event);

/**
 * @brief  Initialize alink config and start alink task
 *         Initialize event loop Create the event handler and task
 *
 * @param  product_info config provice alink init configuration
 *         event_handler_cb application specified event callback
 *
 * @return
 *     - ALINK_OK : Succeed
 *     - ALINK_ERR :   Fail
 */
alink_err_t alink_init(_IN_ const void *product_info,
                       _IN_ const alink_event_cb_t event_handler_cb);

/**
 * @brief  attempts to read up to count bytes from file descriptor fd into the
 *         buffer starting at buf.
 *
 * @param  up_cmd  Store the read data
 * @param  size  Write the size of the data
 * @param  micro_seconds  seconds before the function timeout, set to -1 if wait forever
 *
 * @return
 *     - ALINK_ERR : Error, errno is set appropriately
 *     - Others : Write the size of the data
 */
ssize_t alink_write(_IN_ const void *up_cmd, size_t size, int micro_seconds);

/**
 * @brief  attempts to read up to count bytes from file descriptor fd into the
 *         buffer starting at buf.
 *
 * @param  down_cmd  Store the read data
 * @param  size  Read the size of the data
 * @param  micro_seconds  seconds before the function timeout, set to -1 if wait forever
 *
 * @return
 *     - ALINK_ERR : Error, errno is set appropriately
 *     - Others : Read the size of the data
 */
ssize_t alink_read(_OUT_ void *down_cmd, size_t size, int  micro_seconds);

/**
 * @brief  Clear wifi information, restart the device into the config network mode
 *
 * @return
 *     - ALINK_OK : Succeed
 *     - ALINK_ERR :   Fail
 */
alink_err_t alink_update_router();

/**
 * @brief  Clear all the information of the device and return to the factory status
 *
 * @return
 *     - ALINK_OK : Succeed
 *     - ALINK_ERR :   Fail
 */
alink_err_t alink_factory_setting();

#ifdef __cplusplus
}
#endif

#endif
