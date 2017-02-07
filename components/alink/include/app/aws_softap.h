#ifndef __AWS_CONNECT_H__
#define __AWS_CONNECT_H__
#include "esp_alink.h"
#define SOFTAP_GATEWAY_IP       "172.31.254.250"
#define SOFTAP_TCP_SERVER_PORT      (65125)

alink_err_t aws_softap_init(_OUT_ wifi_config_t * wifi_config);

#endif
