/******************************************************************************
 * Copyright (C) 2014 -2016  Espressif System
 *
 * FileName: app_main.c
 *
 * Description:
 *
 * Modification history:
 * 2016/11/16, v0.0.1 create this file.
*******************************************************************************/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_system.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "string.h"
#include "platform.h"
#include "product.h"

int vendor_connect_ap(char *ssid, char *passwd);
void vendor_wifi_init();
int aws_softap_main(void);
void alink_demo();


int aws_sample(void)
{
    char ssid[32 + 1];
    char passwd[64 + 1];
    char bssid[6];
    char auth;
    char encry;
    char channel;
    int ret;

    char product_model[PRODUCT_MODEL_LEN];
    char product_secret[PRODUCT_SECRET_LEN];
    char device_mac[PLATFORM_MAC_LEN];

    product_get_secret(product_secret);
    product_get_model(product_model);
    platform_wifi_get_mac(device_mac);

    aws_start(product_model, product_secret, device_mac, NULL);
    ret = aws_get_ssid_passwd(&ssid[0], &passwd[0], &bssid[0], &auth, &encry,
                              &channel);
    if (!ret) {
        printf("alink wireless setup timeout!\n");
        ret = -1;
        goto out;
    }
    printf("ssid:%s, passwd:%s\n", ssid, passwd);
    vendor_connect_ap(ssid, passwd);
    aws_notify_app();
    ret = 0;
out:
    aws_destroy();
    return ret;
}


void startdemo_task(void *arg)
{
    alink_demo();
    vTaskDelete(NULL);
}


/******************************************************************************
 * FunctionName : esp_alink_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void esp_alink_init()
{
    int ret = aws_sample(); // alink smart config start
    if (ret == -1) { // alink smarconfig err,enter softap config net mode
        printf("enter softap config net mode\n");
        aws_softap_main();
    }
    xTaskCreate(startdemo_task, "startdemo_task", 4096, NULL, 4, NULL);
}

