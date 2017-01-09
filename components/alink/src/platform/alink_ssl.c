#include <string.h>
#include "openssl/ssl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/netdb.h"

#include "platform/platform.h"

static SSL_CTX *ctx = NULL;
static const char *TAG = "alink_ssl";
#define require_action_exit(con, msg, ...) if(con) {ESP_LOGE(TAG, msg, ##__VA_ARGS__); esp_restart();}
#define require_action_NULL(con, msg, ...) if(con) {ESP_LOGE(TAG, msg, ##__VA_ARGS__); return NULL;}

void *platform_ssl_connect(_IN_ void *tcp_fd, _IN_ const char *server_cert, _IN_ int server_cert_len)
{
    require_action_exit(tcp_fd == NULL, "[%s, %d]:Parameter error tcp_fd == NULL", __func__, __LINE__);
    require_action_exit(server_cert == NULL, "[%s, %d]:Parameter error server_cert == NULL", __func__, __LINE__);

    SSL *ssl;
    int socket = (int)tcp_fd;
    int ret = -1;
    ESP_LOGD(TAG, "[%s, %d]:tcp_fd: %d, server_cert: %p, server_cert_len: %d\n",
             __func__, __LINE__, socket, server_cert, server_cert_len);

    ctx = SSL_CTX_new(TLSv1_1_client_method());
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, socket);
    X509 *ca_cert = d2i_X509(NULL, (unsigned char *)server_cert, server_cert_len);
    require_action_NULL(ca_cert == NULL, "[%s, %d]:d2i_X509", __func__, __LINE__);

    ret = SSL_add_client_CA(ssl, ca_cert);
    require_action_NULL(ret == -1, "[%s, %d]:SSL_add_client_CA", __func__, __LINE__);

    ret = SSL_connect(ssl);
    require_action_NULL(ret == -1, "[%s, %d]:SSL_connect", __func__, __LINE__);

    ESP_LOGD(TAG, "[%s, %d]:ssl: %p", __func__, __LINE__, ssl);
    return (void *)(ssl);
}

int platform_ssl_send(_IN_ void *ssl, _IN_ const char *buffer, _IN_ int length)
{
    require_action_exit(ssl == NULL, "[%s, %d]:Parameter error ssl == NULL", __func__, __LINE__);
    require_action_exit(buffer == NULL, "[%s, %d]:Parameter error buffer == NULL", __func__, __LINE__);

    int cnt = -1;
    cnt = SSL_write((SSL *)ssl, buffer, length);
    return cnt;
    // return (cnt > 0) ? cnt : -1;
}

int platform_ssl_recv(_IN_ void *ssl, _OUT_ char *buffer, _IN_ int length)
{
    require_action_exit(ssl == NULL, "[%s, %d]:Parameter error ssl == NULL", __func__, __LINE__);
    require_action_exit(buffer == NULL, "[%s, %d]:Parameter error buffer == NULL", __func__, __LINE__);
    int cnt = -1;
    cnt = SSL_read((SSL*)ssl, buffer, length);
    return cnt;
    // return cnt > 0 ? cnt : -1;
}

int platform_ssl_close(_IN_ void *ssl)
{
    require_action_exit(ssl == NULL, "[%s, %d]:Parameter error ssl == NULL", __func__, __LINE__);
    int ret = -1;
    ret = SSL_shutdown((SSL *)ssl);
    if (ret != 1) {
        ESP_LOGE(TAG, "[%s, %d]:SSL_shutdown: ret:%d, ssl: %p",
                 __func__, __LINE__, ret, ssl);
        return -1;
    }

    int fd = SSL_get_fd((SSL *)ssl);
    if (fd < 0) {
        ESP_LOGE(TAG, "[%s, %d]:SSL_get_fd:%d", __func__, __LINE__, fd);
        return -1;
    }
    close(fd);

    if (ssl) {
        SSL_free(ssl);
        ssl = NULL;
    }

    if (ctx) {
        SSL_CTX_free(ctx);
        ctx = NULL;
    }

    return 0;
}
