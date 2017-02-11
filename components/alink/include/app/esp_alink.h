#ifndef __ALINK_USER_CONFIG_H__
#define __ALINK_USER_CONFIG_H__
#include "esp_log.h"
#include "alink_export.h"
#include "platform.h"
#include "assert.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#include <stdio.h>
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
#define _IN_            /**< indicate that this is a input parameter. */
#endif
#ifndef _OUT_
#define _OUT_           /**< indicate that this is a output parameter. */
#endif
#ifndef _INOUT_
#define _INOUT_         /**< indicate that this is a io parameter. */
#endif
#ifndef _IN_OPT_
#define _IN_OPT_        /**< indicate that this is a optional input parameter. */
#endif
#ifndef _OUT_OPT_
#define _OUT_OPT_       /**< indicate that this is a optional output parameter. */
#endif
#ifndef _INOUT_OPT_
#define _INOUT_OPT_     /**< indicate that this is a optional io parameter. */
#endif

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL CONFIG_LOG_ALINK_LEVEL

#ifdef CONFIG_ALINK_PASSTHROUGH
#define ALINK_PASSTHROUGH
#endif

#define ALINK_LOGE( format, ... ) ESP_LOGE(TAG, "[%s, %d]:" format, __func__, __LINE__, ##__VA_ARGS__)
#define ALINK_LOGW( format, ... ) ESP_LOGW(TAG, "[%s, %d]:" format, __func__, __LINE__, ##__VA_ARGS__)
#define ALINK_LOGI( format, ... ) ESP_LOGI(TAG, format, ##__VA_ARGS__)
#define ALINK_LOGD( format, ... ) ESP_LOGD(TAG, "[%s, %d]:" format, __func__, __LINE__, ##__VA_ARGS__)
#define ALINK_LOGV( format, ... ) ESP_LOGV(TAG, format, ##__VA_ARGS__)

#define ALINK_ERROR_CHECK(con, err, format, ...) if(con) {ALINK_LOGE(format, ##__VA_ARGS__); perror(__func__); return err;}
#define ALINK_PARAM_CHECK(con) if(con) {ALINK_LOGE("Parameter error, "); assert(0);}

/* alink main */
#define WIFI_WAIT_TIME      (CONFIG_WIFI_WAIT_TIME * 1000 / portTICK_RATE_MS)
#define ALINK_RESET_KEY_IO  CONFIG_ALINK_RESET_KEY_IO
#define DEFAULU_TASK_PRIOTY CONFIG_ALINK_TASK_PRIOTY

/* alink_os */
#define ALINK_CHIPID "esp32"
#define SYSTEM_VERSION "esp32_idf"
#define MODULE_NAME "esp32"


#define ALINK_DATA_LEN 512
/* alink_main */
int esp_write(char *up_cmd, size_t size, TickType_t ticks_to_wait);
int esp_read(char *down_cmd, size_t size, TickType_t ticks_to_wait);

void esp_alink_init(_IN_ const struct device_info *product_info);

// alink_up_cmd_ptr alink_up_cmd_malloc();
// alink_err_t alink_up_cmd_free(_IN_ alink_up_cmd_ptr up_cmd);
// alink_err_t alink_up_cmd_memcpy(_IN_ alink_up_cmd_ptr dest, _OUT_ alink_up_cmd_ptr src);
// alink_down_cmd_ptr alink_down_cmd_malloc();
// alink_err_t alink_down_cmd_free(_IN_ alink_down_cmd_ptr down_cmd);
// alink_err_t alink_down_cmd_memcpy(_IN_ alink_up_cmd_ptr dest, _OUT_ alink_up_cmd_ptr src);
alink_err_t alink_write(alink_up_cmd_ptr up_cmd, TickType_t ticks_to_wait);
alink_err_t alink_read(alink_down_cmd_ptr down_cmd, TickType_t ticks_to_wait);

#endif