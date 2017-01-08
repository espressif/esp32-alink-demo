/**********  include freertos **********/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/portmacro.h"

#include "string.h"
#include "esp_system.h"
#include "esp_err.h"

#include "platform.h"
#include "adapter_layer_config.h"

#define PLATFORM_TABLE_CONTENT_CNT(table) (sizeof(table)/sizeof(table[0]))

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
};
/*
    return -1: not found the name from the list
          !-1: found the pos in the list
*/
int platform_get_task_name_location(char * name)
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

void  platform_get_task_handler(uint32_t pos, void** handler)
{
    ALINK_ADAPTER_CONFIG_ASSERT(pos != -1);
    ALINK_ADAPTER_CONFIG_ASSERT(pos <= PLATFORM_TABLE_CONTENT_CNT(task_infor) - 1);
    *handler = (void*)task_infor[pos].handler;
}

bool platform_save_task_name_handler(uint32_t pos, void * handler)
{
    ALINK_ADAPTER_CONFIG_ASSERT(pos <= PLATFORM_TABLE_CONTENT_CNT(task_infor) - 1);
    task_infor[pos].handler = handler;
    return true;
}

void temp_test(void *arg)
{
    while (1) {
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

/*---------------------------------------------------------------
                    FreeRtos TASK
---------------------------------------------------------------*/
int platform_thread_create(_OUT_ void **thread,
                           _IN_ const char *name,
                           _IN_ void *(*start_routine) (void *),
                           _IN_ void *arg,
                           _IN_ void *stack,
                           _IN_ uint32_t stack_size,
                           _OUT_ int *stack_used)
{
    ALINK_ADAPTER_CONFIG_ASSERT(stack != NULL);
    ALINK_ADAPTER_CONFIG_ASSERT(stack_size != 0);
    printf("task name: %s, stack_size: %d=\n", name, stack_size);

    if (pdTRUE == xTaskCreate(start_routine, name, stack_size*2, arg, ESP_DEFAULU_TASK_PRIOTY, thread)) {
        int pos = platform_get_task_name_location(name);
        if  (-1 != pos) {
            platform_save_task_name_handler(pos, *thread);
        }
        return 0;
    }
    ESP_LOG(ESP_ERROR_LEVEL, "CreatTask");
    return -1;
}

void platform_thread_exit(_IN_ void *thread)
{
    ets_printf("++++++++++++ [%s, %d]: thread:%p ++++++++++\n", __func__, __LINE__,thread);
    vTaskDelete(thread);
}

void platform_msleep(_IN_ uint32_t ms)
{
    ALINK_ADAPTER_CONFIG_ASSERT(portTICK_PERIOD_MS != 0);
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

uint32_t platform_get_time_ms(void)
{
    return system_get_time() / 1000;
}

int platform_thread_get_stack_size(_IN_ const char *thread_name)
{
    if (0 == strcmp(thread_name, "alink_main_thread"))
    {
        printf("get alink_main_thread\n");
        return 0xc00 * 2;
    }
    else if (0 == strcmp(thread_name, "wsf_worker_thread"))
    {
        printf("get wsf_worker_thread\n");
        return 0x2100 * 2;
    }
    else if (0 == strcmp(thread_name, "firmware_upgrade_pthread"))
    {
        printf("get firmware_upgrade_pthread\n");
        return 0xc00 * 2;
    }
    else if (0 == strcmp(thread_name, "send_worker"))
    {
        printf("get send_worker\n");
        return 0x800 * 2;
    }
    else if (0 == strcmp(thread_name, "callback_thread"))
    {
        printf("get callback_thread\n");
        return 0x800 * 2;
    }
    else
    {
        printf("get othrer thread\n");
        return 0x800 * 2;
    }

    assert(0);
}

/*---------------------------------------------------------------
                        FreeRtos Semaphore
---------------------------------------------------------------*/
void *platform_mutex_init(void)
{
    xSemaphoreHandle mux_sem = NULL;
    mux_sem = xSemaphoreCreateMutex();
    if (mux_sem != NULL) {
        // printf("create mux: %p\n", mux_sem);
        return mux_sem;
    }
    ESP_LOG(ESP_ERROR_LEVEL, "Creat Mux");
    return NULL;
}

void platform_mutex_destroy(_IN_ void *mutex)
{
    if (mutex != NULL) {
        vSemaphoreDelete(mutex);
        return;
    }
    ESP_LOG(ESP_ERROR_LEVEL, "delet Mux");
    return ;
}

void platform_mutex_lock(_IN_ void *mutex)
{
    //if can not get the mux,it will wait all the time
    printf("mutex: %p\n", mutex);
    if (mutex == NULL) {
        ESP_LOG(ESP_ERROR_LEVEL, "mutex lock");
        return;
    }
    xSemaphoreTake(mutex, portMAX_DELAY);
}

void platform_mutex_unlock(_IN_ void *mutex)
{
    //give the mux
    printf("mutex unlock: %p\n", mutex);
    if (mutex == NULL) {
        ESP_LOG(ESP_ERROR_LEVEL, "mutex unlock");
        return;
    }
    xSemaphoreGive(mutex);
}

void *platform_semaphore_init(void)
{
    xSemaphoreHandle count_handler = NULL;
    count_handler = xSemaphoreCreateCounting(255, 0);
    if (count_handler != NULL) {
        return count_handler;
    }
    ESP_LOG(ESP_ERROR_LEVEL, "Creat Semphore");
    return NULL;
}

void platform_semaphore_destroy(_IN_ void *sem)
{
    ALINK_ADAPTER_CONFIG_ASSERT(sem != NULL);
    vSemaphoreDelete(sem);
}

int platform_semaphore_wait(_IN_ void *sem, _IN_ uint32_t timeout_ms)
{
    ALINK_ADAPTER_CONFIG_ASSERT(sem != NULL);
    //Take the Semaphore
    if (pdTRUE == xSemaphoreTake(sem, timeout_ms / portTICK_RATE_MS)) {
        return 0;
    } else {
        return -1;
    }

}

void platform_semaphore_post(_IN_ void *sem)
{
    ALINK_ADAPTER_CONFIG_ASSERT(sem != NULL);
    if (sem != NULL) {
        xSemaphoreGive(sem);
        return;
    }
    ESP_LOG(ESP_ERROR_LEVEL, "post sem");
    return;
}

void *platform_malloc(_IN_ uint32_t size)
{
    void * c = NULL;
    c = malloc(size);
    if (c != NULL) {
        return c;
    }
    ESP_LOG(ESP_ERROR_LEVEL, "malloc");
    return NULL;
}

void platform_free(_IN_ void *ptr)
{
    // ALINK_ADAPTER_CONFIG_ASSERT(ptr != NULL);
    if (ptr == NULL) return;
    free(ptr);
    ptr = NULL;

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
