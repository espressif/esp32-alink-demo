#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_types.h"
#include "wpa/common.h"
#include "wpa/ieee802_11_defs.h"
#include "lwip/inet.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "esp_log.h"

#include "platform.h"
#include "alink_export.h"
#include "alink_user_config.h"

static platform_awss_recv_80211_frame_cb_t g_sniffer_cb = NULL;
static const char *TAG = "alink_wifi";

//一键配置超时时间, 建议超时时间1-3min, APP侧一键配置1min超时
int platform_awss_get_timeout_interval_ms(void)
{
    return 60 * 1000;
}

//一键配置每个信道停留时间, 建议200ms-400ms
int platform_awss_get_channelscan_interval_ms(void)
{
    return 200;
}

//wifi信道切换，信道1-13
void platform_awss_switch_channel(char primary_channel,
                                  char secondary_channel, char bssid[ETH_ALEN])
{
    ESP_ERROR_CHECK(esp_wifi_set_channel(primary_channel, secondary_channel));
    //ret = system(buf);
}


struct sniffer_data
{
    uint16_t len;
    char *buf;
};


static void IRAM_ATTR  wifi_sniffer_cb_(void *recv_buf, wifi_promiscuous_pkt_type_t type) {
    char *buf = NULL;
    uint16_t len = 0;
    if (type == WIFI_PKT_CTRL) return;
    wifi_promiscuous_pkt_t *sniffer = (wifi_promiscuous_pkt_t*)recv_buf;
    buf = (char *)sniffer->payload;
    len = sniffer->rx_ctrl.sig_len;
    g_sniffer_cb(buf, len, AWSS_LINK_TYPE_NONE, 1);
}

//进入monitor模式, 并做好一些准备工作，如
//设置wifi工作在默认信道6
//若是linux平台，初始化socket句柄，绑定网卡，准备收包
//若是rtos的平台，注册收包回调函数aws_80211_frame_handler()到系统接口
void platform_awss_open_monitor(_IN_ platform_awss_recv_80211_frame_cb_t cb)
{
    g_sniffer_cb = cb;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_channel(6, 0));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(0));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_cb_));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(1));
}

//退出monitor模式，回到station模式, 其他资源回收
void platform_awss_close_monitor(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(0));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(NULL));
}


int platform_wifi_get_rssi_dbm(void)
{
    wifi_ap_record_t ap_infor;
    esp_wifi_sta_get_ap_info(&ap_infor);
    return ap_infor.rssi;
}

char *platform_wifi_get_mac(_OUT_ char mac_str[PLATFORM_MAC_LEN])
{
    ALINK_PARAM_CHECK(mac_str == NULL);
    uint8_t mac[6] = {0};
    // wifi_mode_t mode_backup;
    // esp_wifi_get_mode(&mode_backup);
    // esp_wifi_set_mode(WIFI_MODE_STA);
    ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_WIFI_STA, mac));
    snprintf(mac_str, PLATFORM_MAC_LEN, MACSTR, MAC2STR(mac));
    // esp_wifi_set_mode(mode_backup);
    return mac_str;
}

uint32_t platform_wifi_get_ip(_OUT_ char ip_str[PLATFORM_IP_LEN])
{
    ALINK_PARAM_CHECK(ip_str == NULL);
    tcpip_adapter_ip_info_t infor;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &infor);
    memcpy(ip_str, inet_ntoa(infor.ip.addr), PLATFORM_IP_LEN);
    return infor.ip.addr;
}

