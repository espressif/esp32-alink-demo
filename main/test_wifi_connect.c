#if 0
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "lwip/sockets.h"
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_connect_test";

static SemaphoreHandle_t xSemConnet = NULL;
static int event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        printf("SYSTEM_EVENT_STA_START\n");
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        printf("SYSTEM_EVENT_STA_GOT_IP\n");
        xSemaphoreGive(xSemConnet);
        // alink_event_send(ALINK_EVENT_WIFI_CONNECTED);
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        printf("SYSTEM_EVENT_STA_DISCONNECTED, free_heap: %d\n", esp_get_free_heap_size());
        // alink_event_send(ALINK_EVENT_WIFI_DISCONNECTED);
        int ret = esp_wifi_connect();
        if (ret != ESP_OK) {
            printf("esp_wifi_connect, ret: %d\n", ret);
        }

        break;

    default:
        break;
    }

    return 0;
}

static void wifi_sniffer_cb(void *recv_buf, wifi_promiscuous_pkt_type_t type)
{
}

static int awss_connect_ap(
    uint32_t connection_timeout_ms,
    char ssid[32],
    char passwd[64])
{
    wifi_config_t wifi_config;

    if (xSemConnet == NULL) {
        xSemConnet = xSemaphoreCreateBinary();
        esp_event_loop_set_cb(event_handler, NULL);
    }
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &wifi_config));
    memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, passwd, sizeof(wifi_config.sta.password));
    printf("ap ssid: %s, password: %s\n", wifi_config.sta.ssid, wifi_config.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    return 0;
}

void app_main()
{
    printf("free heap: %d\n", esp_get_free_heap_size());
    ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(NULL, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // ESP_ERROR_CHECK(esp_wifi_set_promiscuous(0));
    // ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_cb));
    // ESP_ERROR_CHECK(esp_wifi_set_promiscuous(1));
    awss_connect_ap(60000, "IOT_DEMO_TEST", "123456789");
}

#endif
