#if 1
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdarg.h>
#include <string.h>

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "alink_export.h"
#include "platform.h"
#include "product.h"

static SemaphoreHandle_t xSemConnet = NULL;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
	if (xSemConnet == NULL)
		xSemConnet = xSemaphoreCreateBinary();
	switch (event->event_id) {
	case SYSTEM_EVENT_STA_START:
		esp_wifi_connect();
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		xSemaphoreGive(xSemConnet);
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		/* This is a workaround as ESP32 WiFi libs don't currently
		   auto-reassociate. */
		esp_wifi_connect();
		break;
	default:
		break;
	}
	return ESP_OK;
}

/* connect... to ap and got ip */
int vendor_connect_ap(char *ssid, char *passwd)
{
	wifi_config_t wifi_config;
	esp_event_loop_set_cb(event_handler, NULL);

	esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
	memcpy(wifi_config.sta.ssid, ssid, strlen(ssid) + 1);
	memcpy(wifi_config.sta.password, passwd, strlen(passwd) + 1);

	printf("WiFi SSID: %s", wifi_config.ap.ssid);
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
	ESP_ERROR_CHECK( esp_wifi_start() );

	if (xSemConnet == NULL)
		xSemConnet = xSemaphoreCreateBinary();
	return xSemaphoreTake(xSemConnet, portMAX_DELAY);
}


#endif
