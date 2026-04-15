#include "dns_server.h"
#include "app_config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_log.h"

static const char    *TAG       = "DNS";
static TaskHandle_t   s_task    = NULL;
static volatile bool  s_running = false;

/* Minimal DNS header (network byte order) */
typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_hdr_t;

static void dns_task(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed: %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind() failed: %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    /* Receive timeout lets the task notice s_running = false */
    struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* AP IP address — all DNS responses resolve here */
    struct in_addr ap_ip;
    inet_aton(AP_IP_ADDR, &ap_ip);

    uint8_t buf[512], resp[512];
    ESP_LOGI(TAG, "Listening on UDP port %d, resolving all → %s", DNS_PORT, AP_IP_ADDR);

    while (s_running) {
        struct sockaddr_in client;
        socklen_t clen = sizeof(client);

        int n = recvfrom(sock, buf, sizeof(buf), 0,
                         (struct sockaddr *)&client, &clen);
        if (n < (int)sizeof(dns_hdr_t)) continue;  /* timeout or runt packet */

        /* ── Build response ─────────────────────────────────
         * Copy the query's header and question section verbatim,
         * then append a single A-record answer.               */
        dns_hdr_t *req = (dns_hdr_t *)buf;
        dns_hdr_t *rsp = (dns_hdr_t *)resp;

        rsp->id      = req->id;
        rsp->flags   = htons(0x8180);  /* QR=1, AA=1, RA=1, RCODE=0 */
        rsp->qdcount = req->qdcount;
        rsp->ancount = htons(1);
        rsp->nscount = 0;
        rsp->arcount = 0;

        int qlen = n - sizeof(dns_hdr_t);
        memcpy(resp + sizeof(dns_hdr_t), buf + sizeof(dns_hdr_t), qlen);

        /* Answer record appended after question section */
        uint8_t *ans = resp + sizeof(dns_hdr_t) + qlen;
        ans[0]  = 0xC0; ans[1]  = 0x0C;          /* name: ptr to question   */
        ans[2]  = 0x00; ans[3]  = 0x01;           /* type A                  */
        ans[4]  = 0x00; ans[5]  = 0x01;           /* class IN                */
        ans[6]  = 0x00; ans[7]  = 0x00;
        ans[8]  = 0x00; ans[9]  = 0x3C;           /* TTL 60s                 */
        ans[10] = 0x00; ans[11] = 0x04;           /* RDLENGTH 4              */
        memcpy(ans + 12, &ap_ip.s_addr, 4);       /* RDATA: AP IP            */

        sendto(sock, resp, sizeof(dns_hdr_t) + qlen + 16, 0,
               (struct sockaddr *)&client, clen);
    }

    close(sock);
    ESP_LOGI(TAG, "Stopped");
    vTaskDelete(NULL);
}

esp_err_t dns_server_start(void) {
    s_running = true;
    BaseType_t ret = xTaskCreatePinnedToCore(
        dns_task, "DNS", STACK_DNS, NULL, 5, &s_task, 0);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}

void dns_server_stop(void) {
    s_running = false;
    /* Task self-deletes after the next 1s recvfrom timeout */
}
