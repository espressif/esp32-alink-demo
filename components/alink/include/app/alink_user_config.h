#ifndef __ALINK_USER_CONFIG_H__
#define __ALINK_USER_CONFIG_H__
#include "esp_log.h"
#include "alink_export.h"
#include "assert.h"
typedef int32_t alink_err_t;

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#define ALINK_LOGE( format, ... ) ESP_LOGE(TAG, format, ##__VA_ARGS__)
#define ALINK_LOGW( format, ... ) ESP_LOGW(TAG, format, ##__VA_ARGS__)
#define ALINK_LOGI( format, ... ) ESP_LOGI(TAG, format, ##__VA_ARGS__)
#define ALINK_LOGD( format, ... ) ESP_LOGD(TAG, format, ##__VA_ARGS__)
#define ALINK_LOGV( format, ... ) ESP_LOGV(TAG, format, ##__VA_ARGS__)

#define ALINK_ERROR_CHECK(con, err, format, ...) if(con) {ALINK_LOGE("[%s, %d]:" format, __func__, __LINE__, ##__VA_ARGS__); return err;}
#define ALINK_PARAM_CHECK(con) if(con) {ALINK_LOGE("[%s, %d]:Parameter error, ", __func__, __LINE__); assert(0);}

/* alink main */
#define WIFI_WAIT_TIME      (60 * 1000 / portTICK_RATE_MS)
#define ALINK_RESET_KEY_IO 0

/* alink_os */
#define ALINK_CHIPID "rtl8188eu 12345678"
#define SYSTEM_VERSION "esp32_idf_v1.0.0"
#define MODULE_NAME "esp32"

#define DEFAULU_TASK_PRIOTY (tskIDLE_PRIORITY + 4)


#endif
