#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/portmacro.h"
#include "esp_log.h"

#include "string.h"
#include "esp_system.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "platform.h"
#define ALINK_CHIPID "rtl8188eu 12345678"
#define ESP_SYSTEM_VERSION "esp32_idf_v1.0.0"
#define ESP_DEFAULU_TASK_PRIOTY (tskIDLE_PRIORITY + 4)

#define PLATFORM_TABLE_CONTENT_CNT(table) (sizeof(table)/sizeof(table[0]))

static const char *TAG = "alink_os";
#define require_action_exit(con, msg, ...) if(con) {ESP_LOGE(TAG, msg, ##__VA_ARGS__); esp_restart();}
#define require_action_NULL(con, msg, ...) if(con) {ESP_LOGE(TAG, msg, ##__VA_ARGS__); return NULL;}

typedef struct task_name_handler_content {
    const char* task_name;
    void * handler;
} task_infor_t;

typedef enum {
    RESULT_ERROR = -1,
    RESULT_OK = 0,
} esp_platform_result;

task_infor_t task_infor[] = {
    {"wsf_receive_worker", NULL},
    {"wsf_send_worker", NULL},
    {"wsf_callback_worker", NULL},
    {"wsf_worker_thread", NULL},
    {"fota_thread", NULL},
    {"cota_thread", NULL},
    {"alcs_thread", NULL},
    {"alink_main_thread", NULL},
    {"send_worker", NULL},
    {"callback_thread", NULL},
    {"firmware_upgrade_pthread", NULL},
};

void platform_printf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    fflush(stdout);
    // ets_printf(fmt, ##__VA_ARGS__);
}

/************************ memory manage ************************/
void *platform_malloc(_IN_ uint32_t size)
{
    void * c = malloc(size);
    require_action_NULL(c == NULL, "[%s, %d]:malloc size : %d", __func__, __LINE__, size);
    return c;
}

void platform_free(_IN_ void *ptr)
{
    if (ptr == NULL) return;
    free(ptr);
    ptr = NULL;
}

/************************ mutex manage ************************/

void *platform_mutex_init(void)
{
    xSemaphoreHandle mux_sem = NULL;
    mux_sem = xSemaphoreCreateMutex();
    require_action_NULL(mux_sem == NULL, "[%s, %d]:xSemaphoreCreateMutex", __func__, __LINE__);
    return mux_sem;
}

void platform_mutex_destroy(_IN_ void *mutex)
{
    require_action_exit(mutex == NULL, "[%s, %d]:Parameter error mutex == NULL", __func__, __LINE__);
    vSemaphoreDelete(mutex);
}

void platform_mutex_lock(_IN_ void *mutex)
{
    //if can not get the mux,it will wait all the time
    require_action_exit(mutex == NULL, "[%s, %d]:Parameter error mutex == NULL", __func__, __LINE__);
    xSemaphoreTake(mutex, portMAX_DELAY);
}

void platform_mutex_unlock(_IN_ void *mutex)
{
    require_action_exit(mutex == NULL, "[%s, %d]:Parameter error mutex == NULL", __func__, __LINE__);
    xSemaphoreGive(mutex);
}


/************************ semaphore manage ************************/
void *platform_semaphore_init(void)
{
    xSemaphoreHandle count_handler = NULL;
    count_handler = xSemaphoreCreateCounting(255, 0);
    require_action_NULL(count_handler == NULL, "[%s, %d]:xSemaphoreCreateCounting", __func__, __LINE__);
    return count_handler;
}

void platform_semaphore_destroy(_IN_ void *sem)
{
    require_action_exit(sem == NULL, "[%s, %d]:Parameter error sem == NULL", __func__, __LINE__);
    vSemaphoreDelete(sem);
}

int platform_semaphore_wait(_IN_ void *sem, _IN_ uint32_t timeout_ms)
{
    require_action_exit(sem == NULL, "[%s, %d]:Parameter error sem == NULL", __func__, __LINE__);
    //Take the Semaphore
    if (pdTRUE == xSemaphoreTake(sem, timeout_ms / portTICK_RATE_MS)) {
        return 0;
    } else {
        return -1;
    }

}

void platform_semaphore_post(_IN_ void *sem)
{
    require_action_exit(sem == NULL, "[%s, %d]:Parameter error sem == NULL", __func__, __LINE__);
    xSemaphoreGive(sem);
}

void platform_msleep(_IN_ uint32_t ms)
{
    vTaskDelay(ms / portTICK_RATE_MS);
}

uint32_t platform_get_time_ms(void)
{
    return system_get_time() / 1000;
}

int platform_thread_get_stack_size(_IN_ const char *thread_name)
{
    require_action_exit(thread_name == NULL, "[%s, %d]:Parameter error thread_name == NULL", __func__, __LINE__);
    if (0 == strcmp(thread_name, "alink_main_thread")) {
        ESP_LOGI(TAG, "get alink_main_thread");
        return 0xc00;
    } else if (0 == strcmp(thread_name, "wsf_worker_thread")) {
        ESP_LOGI(TAG, "get wsf_worker_thread");
        return 0x2100;
    } else if (0 == strcmp(thread_name, "firmware_upgrade_pthread")) {
        ESP_LOGI(TAG, "get firmware_upgrade_pthread");
        return 0xc00;
    } else if (0 == strcmp(thread_name, "send_worker")) {
        ESP_LOGI(TAG, "get send_worker");
        return 0x800;
    } else if (0 == strcmp(thread_name, "callback_thread")) {
        ESP_LOGI(TAG, "get callback_thread");
        return 0x800;
    } else {
        ESP_LOGI(TAG, "get othrer thread");
        return 0x800;
    }

    assert(0);
}

/************************ task ************************/

/*
    return -1: not found the name from the list
          !-1: found the pos in the list
*/
static int get_task_name_location(const char * name)
{
    uint32_t i = 0;
    uint32_t len = 0;
    for (i = 0; i < PLATFORM_TABLE_CONTENT_CNT(task_infor); i++) {
        len = (strlen(task_infor[i].task_name) >= configMAX_TASK_NAME_LEN ? configMAX_TASK_NAME_LEN : strlen(task_infor[i].task_name));
        if (0 == memcmp(task_infor[i].task_name, name, len)) {
            return i;
        }
    }
    return -1;
}

// static void  platform_get_task_handler(uint32_t pos, void** handler)
// {
//     ALINK_ADAPTER_CONFIG_ASSERT(pos != -1);
//     ALINK_ADAPTER_CONFIG_ASSERT(pos <= PLATFORM_TABLE_CONTENT_CNT(task_infor) - 1);
//     *handler = (void*)task_infor[pos].handler;
// }

static bool set_task_name_handler(uint32_t pos, void * handler)
{
    task_infor[pos].handler = handler;
    return true;
}

int platform_thread_create(_OUT_ void **thread,
                           _IN_ const char *name,
                           _IN_ void *(*start_routine)(void *),
                           _IN_ void *arg,
                           _IN_ void *stack,
                           _IN_ uint32_t stack_size,
                           _OUT_ int *stack_used)
{
    require_action_exit(name == NULL, "[%s, %d]:Parameter error name == NULL", __func__, __LINE__);
    require_action_exit(stack_size == 0, "[%s, %d]:Parameter error stack_size == 0", __func__, __LINE__);
    printf("task name: %s, stack_size: %d=\n", name, stack_size);

    if (pdTRUE == xTaskCreatePinnedToCore(start_routine, name, stack_size*2, arg, ESP_DEFAULU_TASK_PRIOTY, thread, 0)) {
        int pos = get_task_name_location(name);
        if  (-1 != pos) {
            set_task_name_handler(pos, *thread);
        }
        return 0;
    }
    ESP_LOGE(TAG, "[%s, %d]:xTaskCreate", __func__, __LINE__);

    return -1;
}

void platform_thread_exit(_IN_ void *thread)
{
    ets_printf("++++++++++++ [%s, %d]: thread:%p ++++++++++\n", __func__, __LINE__, thread);
    vTaskDelete(thread);
}


/************************ config ************************/
int platform_config_read(_OUT_ char *buffer, _IN_ int length)
{
    require_action_exit(buffer == NULL, "[%s, %d]:Parameter error buffer == NULL", __func__, __LINE__);

    // printf("---------- platform_config_read: %02x %02x %02x length: %d -------------\n",
    //        buffer[0], buffer[1], buffer[2], length);
    esp_err_t ret     = -1;
    nvs_handle config_handle = 0;

    ret = nvs_open("alink", NVS_READWRITE, &config_handle);
    if (ret != 0) return -1;
    ret = nvs_get_blob(config_handle, "config", buffer, (size_t *)&length);
    if (ret != 0) return -1;
    nvs_close(config_handle);
    return 0;
}

int platform_config_write(_IN_ const char *buffer, _IN_ int length)
{
    require_action_exit(buffer == NULL, "[%s, %d]:Parameter error buffer == NULL", __func__, __LINE__);
    // printf("---------- platform_config_write: %02x %02x %02x length: %d -------------\n",
    //        buffer[0], buffer[1], buffer[2], length);

    if (!buffer || length <= 0)
        return -1;

    esp_err_t ret     = -1;
    nvs_handle config_handle = 0;
    ret = nvs_open("alink", NVS_READWRITE, &config_handle);
    if (ret != 0) return -1;
    ret = nvs_set_blob(config_handle, "config", buffer, length);
    if (ret != 0) return -1;
    nvs_commit(config_handle);
    nvs_close(config_handle);
    return 0;
}


char *platform_get_chipid(_OUT_ char cid_str[PLATFORM_CID_LEN])
{
    return strncpy(cid_str, ALINK_CHIPID, PLATFORM_CID_LEN);
}

char *platform_get_os_version(_OUT_ char version_str[PLATFORM_OS_VERSION_LEN])
{
    return strncpy(version_str, ESP_SYSTEM_VERSION, PLATFORM_OS_VERSION_LEN);
}

char *platform_get_module_name(_OUT_ char name_str[PLATFORM_MODULE_NAME_LEN])
{
    strcpy(name_str, "esp32");
    return name_str;
}

int platform_sys_net_is_ready(void)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    return 1;
}

void platform_sys_reboot(void)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    esp_restart();
}


/**
 * @brief release the specified thread resource.
 *
 * @param[in] thread: the specified thread handle.
 * @return None.
 * @see None.
 * @note Called outside of the thread. The resource that must be kept until thread exit completely
 *  can be released here, such as thread stack.
 */
void platform_thread_release_resources(void *thread)
{

}
