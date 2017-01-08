#ifndef __ADAPTER_LAYER_CONFIG_H__
#define __ADAPTER_LAYER_CONFIG_H__
#include "esp_system.h"
#include "rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/****************Common Config******************************/
//AIRLINK_LOG CONFIG
#define ESP_LOG_ENABLE      (1)
#define ESP_LOG_ERROR_ENABLE    (1)
#define ESP_LOG_INFO_ENABLE (1)

#define ESP_ERROR_LEVEL     (1)
#define ESP_INFO_LEVEL          (2)
#define ESP_LOG(level,format,...) do{\
    if(ESP_LOG_ENABLE){\
        if((ESP_LOG_INFO_ENABLE)&&(level==ESP_INFO_LEVEL)){ets_printf("[Infor]");}\
        else if((ESP_LOG_INFO_ENABLE)&&(level==ESP_ERROR_LEVEL)){ets_printf("[Error]");}\
        ets_printf("[%s##%u]"format,__FUNCTION__,__LINE__,##__VA_ARGS__);\
        ets_printf("\r\n");\
    }\
}while(0)

//ALINK_ASSERT
#define ALINK_ADAPTER_CONFIG_ASSERT(a) if(!(a)){\
        ESP_LOG(ESP_ERROR_LEVEL,"");\
        esp_restart();\
    }

/**********************TASK CONFIG**********************/
#define ESP_DEFAULU_TASK_PRIOTY (tskIDLE_PRIORITY+3)

/**********************SOCKET CONFIG********************/



#endif
