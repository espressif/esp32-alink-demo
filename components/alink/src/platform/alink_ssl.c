#include <string.h>
#include "openssl/ssl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/netdb.h"

#include "adapter_layer_config.h"
#include "platform/platform.h"

static SSL_CTX *ctx = NULL;

void *platform_ssl_connect(_IN_ void *tcp_fd, _IN_ const char *server_cert, _IN_ int server_cert_len)
{
    printf("=== [%s, %d] ===\n", __func__, __LINE__);
    printf("tcp_fd: %p, server_cert: %p, server_cert_len: %d\n",
           tcp_fd , server_cert, server_cert_len);
    SSL *ssl;
    int socket = (int)tcp_fd;
    printf("socket: %d\n", socket);
    int ret = -1;
    ALINK_ADAPTER_CONFIG_ASSERT(tcp_fd != NULL);
    ctx = SSL_CTX_new(TLSv1_1_client_method());
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, socket);
    X509 *ca_cert = d2i_X509(NULL, (unsigned char *)server_cert, server_cert_len);
    ALINK_ADAPTER_CONFIG_ASSERT(ca_cert != NULL);
    ret = SSL_add_client_CA(ssl, ca_cert);
    ALINK_ADAPTER_CONFIG_ASSERT(ret != 0);
    ret = SSL_connect(ssl);
    ALINK_ADAPTER_CONFIG_ASSERT(ret != -1);;
    return (void *)(ssl);
}

int platform_ssl_send(_IN_ void *ssl, _IN_ const char *buffer, _IN_ int length)
{
    int cnt = 0;
    ALINK_ADAPTER_CONFIG_ASSERT(ssl != NULL);
    ALINK_ADAPTER_CONFIG_ASSERT(buffer != NULL);
    cnt = SSL_write((SSL*)ssl, buffer, length);
    return (cnt > 0) ? cnt : -1;
}

int platform_ssl_recv(_IN_ void *ssl, _OUT_ char *buffer, _IN_ int length)
{
    printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int cnt = 0;
    ALINK_ADAPTER_CONFIG_ASSERT(ssl != NULL);
    ALINK_ADAPTER_CONFIG_ASSERT(buffer != NULL);
    cnt = SSL_read((SSL*)ssl, buffer, length);
    return cnt > 0 ? cnt : -1;
}

int platform_ssl_close(_IN_ void *ssl)
{
    int ret = -1;
    // ALINK_ADAPTER_CONFIG_ASSERT(ssl != NULL);
    ret = SSL_shutdown((SSL *)ssl);
    if (ret != 0) {
        return -1;
    }

    int fd = SSL_get_fd((SSL *)ssl);
    printf("=== [%s, %d] === ssl: %p ret: %d fd: %d\n", __func__, __LINE__, ssl, ret, fd);
    // ALINK_ADAPTER_CONFIG_ASSERT(fd <= 0);
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
