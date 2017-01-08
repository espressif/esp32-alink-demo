#if 0
#include <string.h>
#include "openssl/ssl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "adapter_layer_config.h"
static SSL_CTX* ssl_ctx = NULL;

void *platform_ssl_connect(_IN_ void *tcp_fd, _IN_ const char *server_cert, _IN_ int server_cert_len)
{
    SSL_CTX *ctx;
    SSL *ssl;

    ctx = SSL_CTX_new(TLSv1_1_client_method());
    if (!ctx) {
        ESP_LOGI(TAG, "failed");
        goto failed1;
    }
    ESP_LOGI(TAG, "OK");

    ret = SSL_CTX_use_certificate_ASN1(ctx, server_cert, server_cert_len);
    if (!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");


    ESP_LOGI(TAG, "create SSL ......");
    ssl = SSL_new(ctx);
    if (!ssl) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

    SSL_set_fd(ssl, tcp_fd);

    ESP_LOGI(TAG, "SSL connected to %s port %d ......",
             OPENSSL_DEMO_TARGET_NAME, OPENSSL_DEMO_TARGET_TCP_PORT);
    ret = SSL_connect(ssl);
    if (!ret) {
        ESP_LOGI(TAG, "failed " );
        goto failed4;
    }
    ESP_LOGI(TAG, "OK");

    return ssl;
}


int platform_ssl_recv(void *ssl, char *buf, int len)
{
    int ret = SSL_read((SSL*)ssl, buf, len);

//    printf("ssl recv %d\n", ret);

    return (ret >= 0) ? ret : -1;
}



int platform_ssl_send(void *ssl, const char *buf, int len)
{
    int i;

    int ret = SSL_write((SSL*)ssl, buf, len);

//    printf("ssl send %d\n", ret);
//
//    for(i = 0; i < 20; ++i)
//    {
//        printf("%5x", *ptr++);
//    }
//    printf("\n\n");

    return (ret > 0) ? ret : -1;
}


int platform_ssl_close(void *ssl)
{
    //SOCKET sock = (SOCKET)SSL_get_fd( ssl );

    SSL_set_shutdown((SSL*)ssl, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
    SSL_free((SSL*)ssl);

    if (ssl_ctx)
    {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
    }

    return 0;
}
#endif
