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
#include "esp_alink.h"

static SSL_CTX *ctx = NULL;
static const char *TAG = "alink_ssl";

void *platform_ssl_connect(_IN_ void *tcp_fd, _IN_ const char *server_cert, _IN_ int server_cert_len)
{
    ALINK_PARAM_CHECK(tcp_fd == NULL);
    ALINK_PARAM_CHECK(server_cert == NULL);

    SSL *ssl;
    int socket = (int)tcp_fd;
    int ret = -1;
    ALINK_LOGD("tcp_fd: %d, server_cert: %p, server_cert_len: %d",
               socket, server_cert, server_cert_len);


    ctx = SSL_CTX_new(TLSv1_1_client_method());
    ALINK_ERROR_CHECK(ctx == NULL, NULL, "SSL_CTX_new, ret:%p", ctx);

    ssl = SSL_new(ctx);
    ALINK_ERROR_CHECK(ssl == NULL, NULL, "SSL_new, ret: %p", ssl);

    SSL_set_fd(ssl, socket);
    X509 *ca_cert = d2i_X509(NULL, (unsigned char *)server_cert, server_cert_len);
    ALINK_ERROR_CHECK(ca_cert == NULL, NULL, "d2i_X509, ret: %p", ca_cert);

    ret = SSL_add_client_CA(ssl, ca_cert);
    ALINK_ERROR_CHECK(ret != pdTRUE, NULL, "SSL_add_client_CA, ret:%d", ret);

    ret = SSL_connect(ssl);
    ALINK_ERROR_CHECK(ret != pdTRUE, NULL, "SSL_connect, ret: %d", ret);

    return (void *)(ssl);
}

int platform_ssl_send(_IN_ void *ssl, _IN_ const char *buffer, _IN_ int length)
{
    ALINK_PARAM_CHECK(length <= 0);
    ALINK_PARAM_CHECK(buffer == NULL);
    // ALINK_PARAM_CHECK(ssl == NULL);
    ALINK_ERROR_CHECK(ssl == NULL, ALINK_ERR, "Parameter error, ssl:%p", ssl);

    alink_err_t ret;
    ret = SSL_write((SSL *)ssl, buffer, length);

    ALINK_ERROR_CHECK(ret <= 0, ALINK_ERR, "SSL_write, ret:%d", ret);
    return ret;
}

int platform_ssl_recv(_IN_ void *ssl, _OUT_ char *buffer, _IN_ int length)
{
    ALINK_PARAM_CHECK(ssl == NULL);
    ALINK_PARAM_CHECK(buffer == NULL);
    int ret = -1;
    ret = SSL_read((SSL*)ssl, buffer, length);
    if (ret <= 0) ALINK_LOGE("SSL_read, ret:%d", ret);
    return ret;
}

int platform_ssl_close(_IN_ void *ssl)
{
    ALINK_PARAM_CHECK(ssl == NULL);
    alink_err_t ret = -1;
    ret = SSL_shutdown((SSL *)ssl);

    if (ret != pdTRUE) {
        ALINK_LOGW("SSL_shutdown: ret:%d, ssl: %p", ret, ssl);
    }

    int fd = SSL_get_fd((SSL *)ssl);

    if (ssl) {
        SSL_free(ssl);
        ssl = NULL;
    }

    if (ctx) {
        SSL_CTX_free(ctx);
        ctx = NULL;
    }

    if (fd >= 0) close(fd);
    else ALINK_LOGE("SSL_get_fd:%d", fd);

    return (ret == pdTRUE) ? ALINK_OK : ALINK_ERR;
}
